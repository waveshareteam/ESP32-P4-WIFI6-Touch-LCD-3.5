#!/usr/bin/env python3
"""Package a verified ESP-IDF revision-profile build into a safe CI artifact."""

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
from pathlib import Path, PurePosixPath
from typing import Any

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BAUD = 460800
FLASH_CAPACITY = 32 * 1024 * 1024
ESP_IMAGE_MAGIC = 0xE9
ESP_IMAGE_HEADER_SIZE = 24
ESP_IMAGE_MAX_SEGMENTS = 16
ESP32P4_IMAGE_CHIP_ID = 18
SAFE_ARTIFACT_TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
FULL_GIT_SHA_RE = re.compile(r"^[0-9a-fA-F]{40}$")
ERASE_TOKEN_RE = re.compile(
    r"(?:^|[^a-z0-9])(?:--?erase(?:[-_]?)(?:all|flash|region)|erase(?:[-_]?)(?:all|flash|region))(?:$|[^a-z0-9])",
    re.IGNORECASE,
)
PROFILE_VALUES = {
    "rev1_3": {
        "CONFIG_ESP32P4_SELECTS_REV_LESS_V3": "y",
        "CONFIG_ESP32P4_REV_MIN_100": "y",
        "CONFIG_ESP32P4_REV_MIN_300": "n",
    },
    "rev3_x": {
        "CONFIG_ESP32P4_SELECTS_REV_LESS_V3": "n",
        "CONFIG_ESP32P4_REV_MIN_100": "n",
        "CONFIG_ESP32P4_REV_MIN_300": "y",
    },
}
C6_IDENTIFIER_RE = re.compile(r"(?:esp32)?c6(?:$|[^a-z0-9])", re.IGNORECASE)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_chip_id(path: Path) -> int | None:
    """Validate image headers when present; raw partition data remains allowed."""
    header = path.read_bytes()[:ESP_IMAGE_HEADER_SIZE]
    if not header or header[0] != ESP_IMAGE_MAGIC:
        return None
    if len(header) < ESP_IMAGE_HEADER_SIZE:
        raise ValueError(f"ESP image header is truncated: {path.name}")
    if not 1 <= header[1] <= ESP_IMAGE_MAX_SEGMENTS:
        raise ValueError(f"ESP image header has an unsafe segment count: {path.name}")
    chip_id = int.from_bytes(header[12:14], byteorder="little")
    if chip_id != ESP32P4_IMAGE_CHIP_ID:
        raise ValueError(f"ESP image header is not ESP32-P4: {path.name}")
    return chip_id


def validate_project_description(build_dir: Path) -> None:
    description = build_dir / "project_description.json"
    if not description.is_file():
        return
    data = load_json(description, "build/project_description.json")
    target = data.get("project_target", data.get("target"))
    if target is not None and str(target).casefold() != "esp32p4":
        raise ValueError("build/project_description.json target is not esp32p4")


def repository_path(raw_path: str, label: str) -> Path:
    path = (REPOSITORY_ROOT / raw_path).resolve()
    try:
        path.relative_to(REPOSITORY_ROOT)
    except ValueError as exc:
        raise ValueError(f"{label} must stay inside the repository") from exc
    return path


def artifact_token(value: str, label: str) -> str:
    if not SAFE_ARTIFACT_TOKEN_RE.fullmatch(value):
        raise ValueError(f"{label} contains unsafe characters")
    return value


def git_sha(value: str) -> str:
    if not FULL_GIT_SHA_RE.fullmatch(value):
        raise ValueError("--git-sha must be a full 40-hex commit SHA")
    return value.lower()


def reject_erase_content(value: Any, label: str = "flasher metadata") -> None:
    """Reject erase operations in every copied metadata shape, including helpers."""
    if isinstance(value, dict):
        for key, item in value.items():
            reject_erase_content(str(key), label)
            reject_erase_content(item, label)
    elif isinstance(value, list):
        for item in value:
            reject_erase_content(item, label)
    elif isinstance(value, str) and ERASE_TOKEN_RE.search(value):
        raise ValueError(f"{label} contains a forbidden erase operation")


def metadata_path_key(raw_path: str) -> str:
    return posixpath.normpath(raw_path.replace("\\", "/"))


def load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError(f"{label} is missing") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"{label} is not valid JSON") from exc
    if not isinstance(data, dict):
        raise ValueError(f"{label} must contain a JSON object")
    return data


def load_flasher_args(build_dir: Path) -> dict[str, Any]:
    data = load_json(build_dir / "flasher_args.json", "build/flasher_args.json")
    if not isinstance(data.get("flash_files"), dict):
        raise ValueError("flasher_args.json does not contain a flash_files map")
    return data


def parse_sdkconfig(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise ValueError(f"profile defaults file is missing: {path}") from exc
    for line in lines:
        if not line.startswith("CONFIG_") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value.strip().strip('"')
    return values


def flatten_config(data: dict[str, Any]) -> dict[str, str]:
    source = data.get("config", data)
    if not isinstance(source, dict):
        raise ValueError("build/config/sdkconfig.json has no config object")
    values: dict[str, str] = {}
    for key, value in source.items():
        if isinstance(value, bool):
            values[key] = "y" if value else "n"
        elif isinstance(value, (str, int)):
            values[key] = str(value).strip().strip('"')
        else:
            # Preserve explicit non-Kconfig values so profile validation rejects them.
            values[key] = str(value).strip().strip('"')
    for key, value in tuple(values.items()):
        if not key.startswith("CONFIG_"):
            values.setdefault(f"CONFIG_{key}", value)
    return values


def effective_config_value(values: dict[str, str], key: str) -> str:
    """Apply Kconfig's implicit disabled-boolean default to JSON build output."""
    return values.get(key, "n")


def reject_c6_content(value: Any, label: str = "flasher metadata") -> None:
    if isinstance(value, dict):
        for key, item in value.items():
            if str(key).casefold() in {"contains_c6_firmware", "contains-c6-firmware"} and item is not False:
                raise ValueError(f"{label} must explicitly exclude ESP32-C6 firmware")
            reject_c6_content(item, label)
    elif isinstance(value, list):
        for item in value:
            reject_c6_content(item, label)
    elif isinstance(value, str) and C6_IDENTIFIER_RE.search(value):
        raise ValueError(f"{label} identifies ESP32-C6 content: {value}")


def validate_profile(project: Path, build_dir: Path, profile: str) -> None:
    expected = PROFILE_VALUES.get(profile)
    if expected is None:
        raise ValueError(f"unknown revision profile: {profile}")
    profile_values = parse_sdkconfig(project / f"sdkconfig.defaults.{profile}")
    effective_values = flatten_config(
        load_json(build_dir / "config" / "sdkconfig.json", "build/config/sdkconfig.json")
    )
    for key, value in expected.items():
        if profile_values.get(key) != value:
            raise ValueError(f"sdkconfig.defaults.{profile} does not define {key}={value}")
        if effective_config_value(effective_values, key) != value:
            raise ValueError(
                f"build/config/sdkconfig.json does not match {profile}: {key}={value}"
            )


def canonical_relative_build_file(raw_file: str) -> PurePosixPath:
    if "\\" in raw_file:
        raise ValueError("flash file paths must use canonical forward slashes")
    path = PurePosixPath(raw_file)
    if (
        not raw_file
        or path.is_absolute()
        or any(part in {"", ".", ".."} for part in path.parts)
        or path.as_posix() != raw_file
    ):
        raise ValueError(f"flash file path is not a canonical safe relative path: {raw_file}")
    return path


def normalize_flash_files(
    build_dir: Path, flash_files: dict[str, Any]
) -> list[tuple[int, str, Path, PurePosixPath]]:
    normalized: list[tuple[int, str, Path, PurePosixPath]] = []
    seen_offsets: set[int] = set()
    ranges: list[tuple[int, int]] = []
    resolved_build = build_dir.resolve()
    for raw_offset, raw_file in flash_files.items():
        offset_text = str(raw_offset)
        try:
            offset = int(offset_text, 0)
        except ValueError as exc:
            raise ValueError(f"invalid flash offset in flasher_args.json: {offset_text}") from exc
        if offset < 0:
            raise ValueError(f"flash offset must be nonnegative: {offset_text}")
        if offset in seen_offsets:
            raise ValueError(f"duplicate flash offset: {offset_text}")
        seen_offsets.add(offset)
        if not isinstance(raw_file, str):
            raise ValueError(f"flash file at {offset_text} is not a path")
        relative_source = canonical_relative_build_file(raw_file)
        source = (resolved_build / relative_source).resolve()
        try:
            source.relative_to(resolved_build)
        except ValueError as exc:
            raise ValueError(f"flash file at {offset_text} escapes the build directory") from exc
        if not source.is_file():
            raise ValueError(f"flash file at {offset_text} is missing from the build output")
        size = source.stat().st_size
        if size <= 0:
            raise ValueError(f"flash file at {offset_text} has zero size")
        end = offset + size
        if end > FLASH_CAPACITY:
            raise ValueError(f"flash range at {offset_text} exceeds the 32 MiB device limit")
        if any(offset < other_end and other_offset < end for other_offset, other_end in ranges):
            raise ValueError(f"flash range at {offset_text} overlaps another flash file")
        ranges.append((offset, end))
        normalized.append((offset, f"0x{offset:x}", source, relative_source))
    if not normalized:
        raise ValueError("flasher_args.json has no flash files")
    return sorted(normalized, key=lambda item: item[0])


def esptool_write_flash_operation(idf_version: str) -> str:
    match = re.match(r"^v?(\d+)(?:\.|$)", idf_version)
    return "write-flash" if match and int(match.group(1)) >= 6 else "write_flash"


def build_flash_command(
    flasher_args: dict[str, Any], target: str, baud: int, idf_version: str
) -> list[str]:
    if target != "esp32p4":
        raise ValueError("only esp32p4 artifacts may be packaged")
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
    write_flash_args = flasher_args.get("write_flash_args", [])
    if not isinstance(write_flash_args, list):
        raise ValueError("write_flash_args must be a list")
    reject_erase_content(write_flash_args, "write_flash_args")
    command.append(esptool_write_flash_operation(idf_version))
    command.extend(str(argument) for argument in write_flash_args)
    return command


def write_flash_helpers(artifact_dir: Path, command: list[str]) -> None:
    if command[:3] != ["python", "-m", "esptool"] or not any(
        token in {"write_flash", "write-flash"} for token in command
    ):
        raise ValueError("flash command must be write-only esptool invocation")
    reject_erase_content(command, "generated flash command")
    esptool_arguments = shlex.join(command[3:])
    (artifact_dir / "flash.sh").write_text(
        "#!/usr/bin/env sh\nset -eu\n"
        'SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\ncd "$SCRIPT_DIR"\n'
        "if command -v python3 >/dev/null 2>&1 && python3 -c 'import esptool' >/dev/null 2>&1; then\n"
        "  PYTHON=python3\n"
        "elif command -v python >/dev/null 2>&1 && python -c 'import esptool' >/dev/null 2>&1; then\n"
        "  PYTHON=python\n"
        "else\n  echo 'Install the esptool module for Python 3 before flashing.' >&2\n  exit 127\nfi\n"
        'exec "$PYTHON" -m esptool ' + esptool_arguments + "\n",
        encoding="utf-8", newline="\n",
    )
    (artifact_dir / "flash.bat").write_text(
        "@echo off\r\nsetlocal\r\ncd /d \"%~dp0\"\r\n"
        + subprocess.list2cmdline(command) + "\r\nif errorlevel 1 exit /b %errorlevel%\r\n",
        encoding="utf-8", newline="",
    )


def rewrite_metadata_file_paths(value: Any, relocations: dict[str, str]) -> Any:
    if isinstance(value, dict):
        return {key: relocations.get(metadata_path_key(item), item) if key == "file" and isinstance(item, str) else rewrite_metadata_file_paths(item, relocations) for key, item in value.items()}
    if isinstance(value, list):
        return [rewrite_metadata_file_paths(item, relocations) for item in value]
    return value


def write_portable_metadata(artifact_dir: Path, flasher_args: dict[str, Any], manifest_files: list[dict[str, Any]], relocations: dict[str, str]) -> None:
    portable_args = rewrite_metadata_file_paths(flasher_args, relocations)
    if not isinstance(portable_args, dict):
        raise ValueError("flasher_args.json root must be a map")
    portable_args["flash_files"] = {entry["offset"]: entry["path"] for entry in manifest_files}
    (artifact_dir / "flasher_args.json").write_text(json.dumps(portable_args, indent=2) + "\n", encoding="utf-8", newline="\n")
    flash_tokens = [str(argument) for argument in flasher_args.get("write_flash_args", [])]
    for entry in manifest_files:
        flash_tokens.extend((entry["offset"], entry["path"]))
    (artifact_dir / "flash_args").write_text("\n".join(flash_tokens) + "\n", encoding="utf-8", newline="\n")


def package(args: argparse.Namespace) -> Path:
    project = repository_path(args.project, "project path")
    if not (project / "CMakeLists.txt").is_file():
        raise ValueError("project path is not an ESP-IDF project root")
    if args.target != "esp32p4":
        raise ValueError("only esp32p4 artifacts may be packaged")
    if args.baud <= 0:
        raise ValueError("baud must be greater than zero")
    profile = artifact_token(args.profile, "revision profile")
    if profile not in PROFILE_VALUES:
        raise ValueError(f"unknown revision profile: {profile}")
    build_name = args.build_dir.replace("\\", "/")
    if build_name != f"build-{profile}" or PurePosixPath(build_name).name != build_name:
        raise ValueError("--build-dir must be the exact profile directory under the project")
    build_dir = (project / build_name).resolve()
    try:
        build_dir.relative_to(project)
    except ValueError as exc:
        raise ValueError("--build-dir must stay under the project") from exc
    validate_profile(project, build_dir, profile)
    validate_project_description(build_dir)
    flasher_args = load_flasher_args(build_dir)
    reject_c6_content(flasher_args)
    reject_erase_content(flasher_args)
    normalized_git_sha = git_sha(args.git_sha)
    flash_files = normalize_flash_files(build_dir, flasher_args["flash_files"])
    # Validate source image headers before creating an artifact directory, so a
    # rejected C6/malformed image cannot leave a partial package behind.
    source_image_ids = {source: image_chip_id(source) for _, _, source, _ in flash_files}
    if ESP32P4_IMAGE_CHIP_ID not in source_image_ids.values():
        raise ValueError("flash plan must include at least one valid ESP32-P4 image")
    idf_version = artifact_token(args.idf_version, "ESP-IDF version")
    output_root = repository_path(args.output_root, "output path")
    artifact_dir = (output_root / f"{idf_version}-{project.name}-{profile}").resolve()
    try:
        artifact_dir.relative_to(output_root)
    except ValueError as exc:
        raise ValueError("artifact directory must stay inside the output path") from exc
    artifact_dir.mkdir(parents=True, exist_ok=False)
    manifest_files: list[dict[str, Any]] = []
    relocations: dict[str, str] = {}
    command = build_flash_command(flasher_args, args.target, args.baud, idf_version)
    p4_image_count = 0
    for offset, offset_text, source, relative_source in flash_files:
        destination_relative = PurePosixPath("bin") / relative_source
        destination = artifact_dir / destination_relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        artifact_path = destination_relative.as_posix()
        chip_id = image_chip_id(destination)
        if chip_id == ESP32P4_IMAGE_CHIP_ID:
            p4_image_count += 1
        manifest_files.append({"offset": offset_text, "path": artifact_path, "size": destination.stat().st_size, "sha256": file_sha256(destination), "image_chip_id": chip_id})
        relocations[relative_source.as_posix()] = artifact_path
        command.extend((offset_text, artifact_path))
    if p4_image_count < 1:
        raise ValueError("flash plan must include at least one valid ESP32-P4 image")
    write_portable_metadata(artifact_dir, flasher_args, manifest_files, relocations)
    manifest = {
        "schema_version": 2,
        "artifact_kind": "esp-idf-flashable",
        "profile": profile,
        "host_only": True,
        "contains_c6_firmware": False,
        "name": project.name,
        "framework": "ESP-IDF",
        "framework_version": idf_version,
        "target": args.target,
        "project_path": project.relative_to(REPOSITORY_ROOT).as_posix(),
        "git_sha": normalized_git_sha,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "baud": args.baud,
        "files": manifest_files,
        "flash_command": shlex.join(command),
    }
    (artifact_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    (artifact_dir / "README.txt").write_text("This archive was generated from a verified ESP-IDF revision-profile CI build.\nReview manifest.json before flashing.\n", encoding="utf-8", newline="\n")
    write_flash_helpers(artifact_dir, command)
    return artifact_dir


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", required=True)
    parser.add_argument("--idf-version", required=True)
    parser.add_argument("--profile", required=True, choices=sorted(PROFILE_VALUES))
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--git-sha", required=True)
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
