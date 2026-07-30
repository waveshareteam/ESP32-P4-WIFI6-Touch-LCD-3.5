#!/usr/bin/env python3
"""Package an ESP-IDF build into a repeatable flashable CI artifact."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import posixpath
import re
import shlex
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BAUD = 460800
SAFE_ARTIFACT_TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def repository_path(raw_path: str, label: str) -> Path:
    path = (REPOSITORY_ROOT / raw_path).resolve()
    try:
        path.relative_to(REPOSITORY_ROOT)
    except ValueError as exc:
        raise ValueError(f"{label} must stay inside the repository") from exc
    return path


def artifact_token(value: str, label: str) -> str:
    if not SAFE_ARTIFACT_TOKEN_RE.fullmatch(value):
        raise ValueError(
            f"{label} must start with an alphanumeric character and contain only "
            "letters, numbers, dots, underscores, or hyphens"
        )
    return value


def metadata_path_key(raw_path: str) -> str:
    return posixpath.normpath(raw_path.replace("\\", "/"))


def load_flasher_args(build_dir: Path) -> dict[str, Any]:
    flasher_path = build_dir / "flasher_args.json"
    try:
        data = json.loads(flasher_path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError("build/flasher_args.json is missing") from exc
    except json.JSONDecodeError as exc:
        raise ValueError("build/flasher_args.json is not valid JSON") from exc

    if not isinstance(data, dict) or not isinstance(data.get("flash_files"), dict):
        raise ValueError("flasher_args.json does not contain a flash_files map")
    return data


def normalize_flash_files(
    build_dir: Path, flash_files: dict[str, Any]
) -> list[tuple[str, Path, Path, str]]:
    normalized: list[tuple[str, Path, Path, str]] = []
    resolved_build = build_dir.resolve()

    for raw_offset, raw_file in flash_files.items():
        offset = str(raw_offset)
        try:
            int(offset, 0)
        except ValueError as exc:
            raise ValueError(f"invalid flash offset in flasher_args.json: {offset}") from exc
        if not isinstance(raw_file, str):
            raise ValueError(f"flash file at {offset} is not a path")

        source = Path(raw_file)
        if not source.is_absolute():
            source = resolved_build / source
        source = source.resolve()
        try:
            relative_source = source.relative_to(resolved_build)
        except ValueError as exc:
            raise ValueError(f"flash file at {offset} escapes the build directory") from exc
        if not source.is_file():
            raise ValueError(f"flash file at {offset} is missing from the build output")
        normalized.append((offset, source, relative_source, raw_file))

    return sorted(normalized, key=lambda item: int(item[0], 0))


def build_flash_command(
    flasher_args: dict[str, Any], target: str, baud: int
) -> list[str]:
    command = ["python", "-m", "esptool", "--chip", target, "--baud", str(baud)]
    extra_args = flasher_args.get("extra_esptool_args", {})
    if not isinstance(extra_args, dict):
        raise ValueError("extra_esptool_args must be a map")

    for option in ("before", "after"):
        value = extra_args.get(option)
        if value is not None:
            command.extend((f"--{option}", str(value)))
    if extra_args.get("stub") is False:
        command.append("--no-stub")

    command.append("write_flash")
    write_flash_args = flasher_args.get("write_flash_args", [])
    if not isinstance(write_flash_args, list):
        raise ValueError("write_flash_args must be a list")
    command.extend(str(argument) for argument in write_flash_args)
    return command


def write_flash_helpers(artifact_dir: Path, command: list[str]) -> None:
    if command[:3] != ["python", "-m", "esptool"]:
        raise ValueError("flash command must use the expected esptool module prefix")
    esptool_arguments = shlex.join(command[3:])
    shell_script = artifact_dir / "flash.sh"
    shell_script.write_text(
        "#!/usr/bin/env sh\n"
        "set -eu\n"
        'SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        'cd "$SCRIPT_DIR"\n'
        "if command -v python3 >/dev/null 2>&1 && python3 -c 'import esptool' >/dev/null 2>&1; then\n"
        "  PYTHON=python3\n"
        "elif command -v python >/dev/null 2>&1 && python -c 'import esptool' >/dev/null 2>&1; then\n"
        "  PYTHON=python\n"
        "else\n"
        '  echo "Install the esptool module for Python 3 before flashing." >&2\n'
        "  exit 127\n"
        "fi\n"
        f'exec "$PYTHON" -m esptool {esptool_arguments}\n',
        encoding="utf-8",
        newline="\n",
    )
    shell_script.chmod(0o755)

    batch_script = artifact_dir / "flash.bat"
    batch_script.write_text(
        "@echo off\r\n"
        "setlocal\r\n"
        'cd /d "%~dp0"\r\n'
        f"{subprocess.list2cmdline(command)}\r\n"
        "if errorlevel 1 exit /b %errorlevel%\r\n",
        encoding="utf-8",
        newline="",
    )


def rewrite_metadata_file_paths(value: Any, relocations: dict[str, str]) -> Any:
    if isinstance(value, dict):
        rewritten: dict[str, Any] = {}
        for key, item in value.items():
            if key == "file" and isinstance(item, str):
                rewritten[key] = relocations.get(metadata_path_key(item), item)
            else:
                rewritten[key] = rewrite_metadata_file_paths(item, relocations)
        return rewritten
    if isinstance(value, list):
        return [rewrite_metadata_file_paths(item, relocations) for item in value]
    return value


def write_portable_metadata(
    artifact_dir: Path,
    flasher_args: dict[str, Any],
    manifest_files: list[dict[str, Any]],
    relocations: dict[str, str],
) -> None:
    portable_args = rewrite_metadata_file_paths(flasher_args, relocations)
    if not isinstance(portable_args, dict):
        raise ValueError("flasher_args.json root must be a map")
    portable_args["flash_files"] = {
        entry["offset"]: entry["path"] for entry in manifest_files
    }
    (artifact_dir / "flasher_args.json").write_text(
        json.dumps(portable_args, indent=2) + "\n", encoding="utf-8", newline="\n"
    )

    write_flash_args = flasher_args.get("write_flash_args", [])
    flash_tokens = [str(argument) for argument in write_flash_args]
    for entry in manifest_files:
        flash_tokens.extend((entry["offset"], entry["path"]))
    (artifact_dir / "flash_args").write_text(
        "\n".join(flash_tokens) + "\n", encoding="utf-8", newline="\n"
    )


def package(args: argparse.Namespace) -> Path:
    project = repository_path(args.project, "project path")
    if not (project / "CMakeLists.txt").is_file():
        raise ValueError("project path is not an ESP-IDF project root")
    if args.baud <= 0:
        raise ValueError("baud must be greater than zero")

    build_dir = project / "build"
    flasher_args = load_flasher_args(build_dir)
    flash_files = normalize_flash_files(build_dir, flasher_args["flash_files"])

    idf_version = artifact_token(args.idf_version, "ESP-IDF version")
    output_root = repository_path(args.output_root, "output path")
    artifact_name = f"{idf_version}-{project.name}"
    artifact_dir = (output_root / artifact_name).resolve()
    try:
        artifact_dir.relative_to(output_root)
    except ValueError as exc:
        raise ValueError("artifact directory must stay inside the output path") from exc
    artifact_dir.mkdir(parents=True, exist_ok=False)

    manifest_files: list[dict[str, Any]] = []
    relocations: dict[str, str] = {}
    command = build_flash_command(flasher_args, args.target, args.baud)
    for offset, source, relative_source, raw_source in flash_files:
        destination_relative = Path("bin") / relative_source
        destination = artifact_dir / destination_relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        artifact_path = destination_relative.as_posix()
        manifest_files.append(
            {
                "offset": offset,
                "path": artifact_path,
                "size": destination.stat().st_size,
                "sha256": file_sha256(destination),
            }
        )
        for source_name in (
            raw_source,
            relative_source.as_posix(),
            source.as_posix(),
        ):
            source_key = metadata_path_key(source_name)
            previous = relocations.get(source_key)
            if previous is not None and previous != artifact_path:
                raise ValueError(f"conflicting flash file metadata path: {source_name}")
            relocations[source_key] = artifact_path
        command.extend((offset, artifact_path))

    write_portable_metadata(artifact_dir, flasher_args, manifest_files, relocations)
    manifest = {
        "name": project.name,
        "framework": "ESP-IDF",
        "framework_version": idf_version,
        "target": args.target,
        "project_path": project.relative_to(REPOSITORY_ROOT).as_posix(),
        "git_sha": os.environ.get("GITHUB_SHA", "unknown"),
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "baud": args.baud,
        "files": manifest_files,
        "flash_command": shlex.join(command),
    }
    (artifact_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    (artifact_dir / "README.txt").write_text(
        "This archive was generated from a successful ESP-IDF CI build.\n"
        "Install esptool and connect the board before flashing.\n"
        "On POSIX systems run: sh flash.sh\n"
        "On Windows run: flash.bat\n"
        "Review manifest.json before flashing to confirm the project and target.\n",
        encoding="utf-8",
        newline="\n",
    )
    write_flash_helpers(artifact_dir, command)
    return artifact_dir


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", required=True)
    parser.add_argument("--idf-version", required=True)
    parser.add_argument("--target", default="esp32p4")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--output-root", default="ci-artifacts")
    args = parser.parse_args()

    try:
        artifact_dir = package(args)
    except (OSError, ValueError) as exc:
        print(f"ESP-IDF artifact packaging failed: {exc}", file=sys.stderr)
        return 2

    print(f"Packaged {artifact_dir.relative_to(REPOSITORY_ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
