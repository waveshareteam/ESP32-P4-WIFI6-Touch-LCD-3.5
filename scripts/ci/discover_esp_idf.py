#!/usr/bin/env python3
"""Discover first-party ESP-IDF projects for the CI build matrix."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path, PurePosixPath

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
EXAMPLES_ROOT = REPOSITORY_ROOT / "examples" / "esp-idf"
SAFE_PROJECT_NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


def discover_projects() -> list[Path]:
    """Return direct child directories that are ESP-IDF projects."""
    try:
        examples_root = EXAMPLES_ROOT.resolve()
        projects = sorted(
            path
            for path in EXAMPLES_ROOT.iterdir()
            if path.is_dir() and (path / "CMakeLists.txt").is_file()
        )
        escaped_projects = [
            project.name
            for project in projects
            if project.is_symlink() or project.resolve().parent != examples_root
        ]
    except (OSError, RuntimeError) as exc:
        raise ValueError(f"cannot inspect examples/esp-idf: {exc}") from exc

    if escaped_projects:
        rendered = ", ".join(repr(name) for name in escaped_projects)
        raise ValueError(
            f"project directories must be real direct children of the examples root: {rendered}"
        )
    unsafe_names = [
        project.name
        for project in projects
        if not SAFE_PROJECT_NAME_RE.fullmatch(project.name)
    ]
    if unsafe_names:
        rendered = ", ".join(repr(name) for name in unsafe_names)
        raise ValueError(f"project directory names contain unsupported characters: {rendered}")
    if not projects:
        raise ValueError("no first-party ESP-IDF projects were found")
    return projects


def resolve_selector(selector: str, projects: list[Path]) -> list[Path]:
    """Resolve all, an example directory name, or a repository-relative path."""
    cleaned = selector.strip().replace("\\", "/").rstrip("/")
    if not cleaned or cleaned.casefold() == "all":
        return projects

    exact_names = {project.name: project for project in projects}
    if cleaned in exact_names:
        return [exact_names[cleaned]]

    folded_names = {project.name.casefold(): project for project in projects}
    if cleaned.casefold() in folded_names:
        return [folded_names[cleaned.casefold()]]

    relative = PurePosixPath(cleaned)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError("selector must be a repository-relative example path")

    candidate = (REPOSITORY_ROOT / Path(*relative.parts)).resolve()
    examples_root = EXAMPLES_ROOT.resolve()
    try:
        within_examples = candidate.relative_to(examples_root)
    except ValueError as exc:
        raise ValueError("selector must be inside examples/esp-idf") from exc

    if not within_examples.parts:
        raise ValueError("selector must identify one example, not the examples root")

    project_name = within_examples.parts[0]
    project = exact_names.get(project_name)
    if project is None:
        raise ValueError(f"selector does not belong to a first-party project: {cleaned}")
    if not candidate.exists():
        raise ValueError(f"selected repository path does not exist: {cleaned}")
    return [project]


def matrix_json(projects: list[Path]) -> str:
    matrix = [
        {
            "name": project.name,
            "path": project.relative_to(REPOSITORY_ROOT).as_posix(),
        }
        for project in projects
    ]
    return json.dumps(matrix, separators=(",", ":"))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Discover direct child ESP-IDF projects for GitHub Actions."
    )
    parser.add_argument(
        "selector",
        nargs="?",
        default="all",
        help="all, an example directory name, or a repository-relative path",
    )
    parser.add_argument(
        "--github-output",
        type=Path,
        help="append examples and count outputs to the GitHub Actions output file",
    )
    args = parser.parse_args()

    try:
        selected = resolve_selector(args.selector, discover_projects())
    except ValueError as exc:
        print(f"ESP-IDF discovery failed: {exc}", file=sys.stderr)
        return 2

    encoded = matrix_json(selected)
    if args.github_output:
        with args.github_output.open("a", encoding="utf-8", newline="\n") as output:
            output.write(f"examples={encoded}\n")
            output.write(f"count={len(selected)}\n")
        print(f"Selected {len(selected)} first-party ESP-IDF project(s).")
    else:
        print(encoded)
    return 0


if __name__ == "__main__":
    sys.exit(main())
