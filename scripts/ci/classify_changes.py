#!/usr/bin/env python3
"""Route repository changes to the affected first-party ESP-IDF examples.

The classifier fails closed when it cannot obtain a complete change set. It
keeps documentation and delivered firmware out of the expensive example build
matrix, selects one project for project-local source changes, and selects every
project for shared, workflow, or otherwise unknown build-impacting changes.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath

from discover_esp_idf import (
    REPOSITORY_ROOT,
    discover_projects,
    matrix_json,
    resolve_selector,
)

CANONICAL_EXAMPLE_ROOT = PurePosixPath("examples/esp-idf")
LEGACY_EXAMPLE_ROOT = PurePosixPath("example/ESP-IDF")
FIRMWARE_ROOT = PurePosixPath("firmware")
RELEASE_ROOT = PurePosixPath("releases")

DOCUMENT_EXTENSIONS = {".md", ".mdx", ".rst", ".adoc"}
DOCUMENT_ASSET_EXTENSIONS = {
    ".bmp",
    ".gif",
    ".jpeg",
    ".jpg",
    ".pdf",
    ".png",
    ".svg",
    ".webp",
}
DELIVERY_EXTENSIONS = {".bin", ".zip"}
ROOT_DOCUMENT_NAMES = {
    ".gitignore",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "CONTRIBUTING_ZH.md",
    "LICENSE",
    "LICENSE.md",
    "LICENSE.txt",
    "README.md",
    "README_ZH.md",
    "SECURITY.md",
    "SECURITY_ZH.md",
    "SUPPORT.md",
    "SUPPORT_ZH.md",
    ".github/pull_request_template.md",
}
GLOBAL_BUILD_FILES = {
    ".gitmodules",
    "CMakeLists.txt",
    "Flash-CI-Firmware.cmd",
    "dependencies.lock",
    "idf_component.yml",
    "partitions.csv",
    "scripts/Flash-CI-Firmware.ps1",
    "sdkconfig",
    "sdkconfig.defaults",
}
DOCUMENTATION_POLICY_FILES = {
    "config/ci-routing.json",
    "config/markdown-audit.json",
    "scripts/check_readme.py",
}
GLOBAL_BUILD_PREFIXES = (
    PurePosixPath(".github/workflows"),
    PurePosixPath("components"),
    PurePosixPath("config"),
    PurePosixPath("scripts/ci"),
)
DOCUMENT_PREFIXES = (
    PurePosixPath(".github/ISSUE_TEMPLATE"),
    PurePosixPath("assets"),
    PurePosixPath("docs"),
    PurePosixPath("example"),
    PurePosixPath("schematic"),
)
SHARED_EXAMPLE_NAMES = {"common", "components", "shared", "_shared"}
STATUS_RE = re.compile(r"^(?:[ACDMRTUXB][0-9]*|[?]{2})$")


class ClassificationError(RuntimeError):
    """The requested scope could not be classified completely."""


@dataclass(frozen=True)
class Change:
    status: str
    paths: tuple[str, ...]


@dataclass(frozen=True)
class Route:
    path: str
    status: str
    kind: str
    build: str
    reason: str
    docs_only: bool


def is_under(path: PurePosixPath, prefix: PurePosixPath) -> bool:
    return path == prefix or path.parts[: len(prefix.parts)] == prefix.parts


def normalize_path(raw_path: str) -> str:
    cleaned = raw_path.strip().replace("\\", "/").strip("/")
    path = PurePosixPath(cleaned)
    if not cleaned or path.is_absolute() or ".." in path.parts:
        raise ClassificationError(f"unsafe or empty repository path: {raw_path!r}")
    return path.as_posix()


def parse_name_status(lines: list[str]) -> list[Change]:
    changes: list[Change] = []
    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.rstrip("\r\n")
        if not line:
            continue
        fields = line.split("\t")
        if len(fields) == 1 or not STATUS_RE.fullmatch(fields[0]):
            changes.append(Change("M", (normalize_path(line),)))
            continue

        status = fields[0]
        path_count = 2 if status.startswith(("R", "C")) else 1
        if len(fields) != path_count + 1:
            raise ClassificationError(
                f"invalid name-status record on line {line_number}: {line!r}"
            )
        changes.append(
            Change(status, tuple(normalize_path(value) for value in fields[1:]))
        )
    if not changes:
        raise ClassificationError("the changed-file scope is empty")
    return changes


def run_git(repository_root: Path, args: list[str]) -> str:
    completed = subprocess.run(
        ["git", "-C", str(repository_root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
    )
    if completed.returncode:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise ClassificationError(f"git {' '.join(args)} failed: {detail}")
    return completed.stdout


def git_range_changes(repository_root: Path, base: str, head: str) -> list[Change]:
    if not base or not head:
        raise ClassificationError("both base and head revisions are required")
    if set(base) == {"0"}:
        tracked = run_git(repository_root, ["ls-files"]).splitlines()
        return parse_name_status([f"A\t{path}" for path in tracked])

    run_git(repository_root, ["rev-parse", "--verify", f"{base}^{{commit}}"])
    run_git(repository_root, ["rev-parse", "--verify", f"{head}^{{commit}}"])
    output = run_git(
        repository_root,
        ["diff", "--name-status", "--find-renames", f"{base}...{head}"],
    )
    return parse_name_status(output.splitlines())


def working_tree_changes(repository_root: Path) -> list[Change]:
    tracked = run_git(
        repository_root,
        ["diff", "--name-status", "--find-renames", "HEAD"],
    ).splitlines()
    untracked = run_git(
        repository_root,
        ["ls-files", "--others", "--exclude-standard"],
    ).splitlines()
    return parse_name_status([*tracked, *(f"A\t{path}" for path in untracked)])


def is_document_path(path: PurePosixPath) -> bool:
    suffix = path.suffix.casefold()
    name = path.name.casefold()
    return (
        suffix in DOCUMENT_EXTENSIONS | DOCUMENT_ASSET_EXTENSIONS
        or (suffix == ".txt" and name.startswith(("readme", "changelog", "notice")))
        or name in {"license", "notice"}
    )


def route_path(path_text: str, status: str, project_names: set[str]) -> Route:
    path = PurePosixPath(path_text)
    suffix = path.suffix.casefold()

    if is_under(path, FIRMWARE_ROOT):
        docs_only = suffix in DOCUMENT_EXTENSIONS | DOCUMENT_ASSET_EXTENSIONS
        kind = "firmware_documentation" if docs_only else "firmware_delivery_or_source"
        return Route(
            path_text,
            status,
            kind,
            "none",
            "firmware is a maintainer-directed delivery surface outside example CI",
            docs_only,
        )

    if is_under(path, RELEASE_ROOT):
        docs_only = suffix in DOCUMENT_EXTENSIONS | DOCUMENT_ASSET_EXTENSIONS
        return Route(
            path_text,
            status,
            "release_documentation" if docs_only else "release_input",
            "none",
            "release delivery is reviewed separately from example CI",
            docs_only,
        )

    for examples_root in (CANONICAL_EXAMPLE_ROOT, LEGACY_EXAMPLE_ROOT):
        if not is_under(path, examples_root):
            continue
        relative_parts = path.parts[len(examples_root.parts) :]
        if not relative_parts:
            return Route(
                path_text,
                status,
                "shared_example_root",
                "all",
                "the framework example root affects discovery for every project",
                False,
            )

        project_name = relative_parts[0]
        if is_document_path(path):
            return Route(
                path_text,
                status,
                "example_documentation",
                "none",
                "documentation beside an example does not change its build",
                True,
            )

        if (
            project_name == "11_esp_brookesia_phone"
            and tuple(relative_parts[1:4]) == ("components", "brookesia_core", "test_apps")
        ):
            return Route(
                path_text,
                status,
                "embedded_upstream_test_app",
                "none",
                "embedded upstream test applications are not product examples",
                False,
            )

        if project_name in SHARED_EXAMPLE_NAMES:
            return Route(
                path_text,
                status,
                "shared_example_source",
                "all",
                "shared framework source can affect every first-party project",
                False,
            )
        if project_name in project_names:
            return Route(
                path_text,
                status,
                "project_source",
                project_name,
                "project-local source or configuration selects that project",
                False,
            )
        return Route(
            path_text,
            status,
            "unknown_example_path",
            "all",
            "an unfamiliar or removed example path is handled conservatively",
            False,
        )

    if path_text in DOCUMENTATION_POLICY_FILES:
        return Route(
            path_text,
            status,
            "documentation_validator",
            "none",
            "documentation policy validation does not change product examples",
            True,
        )

    if path_text in GLOBAL_BUILD_FILES or any(
        is_under(path, prefix) for prefix in GLOBAL_BUILD_PREFIXES
    ):
        return Route(
            path_text,
            status,
            "global_build_input",
            "all",
            "workflow, CI helper, shared component, or root build input affects all projects",
            False,
        )

    if path.parts[:2] == ("scripts", "tests"):
        return Route(
            path_text,
            status,
            "documentation_validator",
            "none",
            "documentation validation does not change product examples",
            True,
        )

    if path_text in ROOT_DOCUMENT_NAMES or path.name.startswith("README") or any(
        is_under(path, prefix) for prefix in DOCUMENT_PREFIXES
    ):
        return Route(
            path_text,
            status,
            "documentation_or_governance",
            "none",
            "documentation, governance, assets, and hardware references do not require a build",
            True,
        )

    return Route(
        path_text,
        status,
        "unknown",
        "all",
        "unknown non-document paths are handled conservatively",
        False,
    )


def classify_changes(changes: list[Change]) -> dict[str, object]:
    projects = discover_projects()
    project_names = {project.name for project in projects}
    routes: list[Route] = []
    seen: set[tuple[str, str]] = set()
    for change in changes:
        for changed_path in change.paths:
            key = (change.status, changed_path)
            if key in seen:
                continue
            seen.add(key)
            routes.append(route_path(changed_path, change.status, project_names))

    if not routes:
        raise ClassificationError("the changed-file scope produced no routable paths")

    select_all = any(route.build == "all" for route in routes)
    selected_names = {
        route.build
        for route in routes
        if route.build not in {"all", "none"}
    }
    if select_all:
        selected = projects
        mode = "all"
    elif selected_names:
        selected = [project for project in projects if project.name in selected_names]
        mode = "selected"
    else:
        selected = []
        mode = "none"

    changed_paths = sorted({route.path for route in routes})
    unknown_paths = sorted(
        route.path
        for route in routes
        if route.kind in {"unknown", "unknown_example_path"}
    )
    delivery_paths = sorted(
        route.path
        for route in routes
        if PurePosixPath(route.path).suffix.casefold() in DELIVERY_EXTENSIONS
    )
    return {
        "schema_version": 1,
        "scope": {
            "changed_files": len(changed_paths),
            "impact_paths": changed_paths,
            "docs_only": all(route.docs_only for route in routes),
            "example_build_required": bool(selected),
            "firmware_touched": any(is_under(PurePosixPath(path), FIRMWARE_ROOT) for path in changed_paths),
            "release_review_required": bool(delivery_paths),
            "delivery_paths": delivery_paths,
        },
        "esp_idf": {
            "mode": mode,
            "selected": json.loads(matrix_json(selected)),
            "available": json.loads(matrix_json(projects)),
        },
        "routes": [asdict(route) for route in routes],
        "unknown_paths": unknown_paths,
    }


def classify_selector(selector: str) -> dict[str, object]:
    projects = discover_projects()
    selected = resolve_selector(selector, projects)
    mode = "all" if len(selected) == len(projects) else "selected"
    return {
        "schema_version": 1,
        "scope": {
            "changed_files": 0,
            "impact_paths": [],
            "docs_only": False,
            "example_build_required": True,
            "firmware_touched": False,
            "release_review_required": False,
            "delivery_paths": [],
        },
        "esp_idf": {
            "mode": mode,
            "selected": json.loads(matrix_json(selected)),
            "available": json.loads(matrix_json(projects)),
        },
        "routes": [],
        "unknown_paths": [],
    }


def append_github_outputs(path: Path, report: dict[str, object]) -> None:
    scope = report["scope"]
    esp_idf = report["esp_idf"]
    selected = esp_idf["selected"]
    with path.open("a", encoding="utf-8", newline="\n") as output:
        output.write(f"mode={esp_idf['mode']}\n")
        output.write(f"examples={json.dumps(selected, separators=(',', ':'))}\n")
        output.write(f"count={len(selected)}\n")
        output.write(f"docs_only={str(scope['docs_only']).lower()}\n")
        output.write(f"firmware_touched={str(scope['firmware_touched']).lower()}\n")
        output.write(
            "release_review_required="
            f"{str(scope['release_review_required']).lower()}\n"
        )
        output.write(f"unknown_count={len(report['unknown_paths'])}\n")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Classify a complete Git diff and select affected ESP-IDF examples."
    )
    scope = parser.add_mutually_exclusive_group(required=True)
    scope.add_argument("--base", help="base Git revision for a merge-base diff")
    scope.add_argument("--working-tree", action="store_true")
    scope.add_argument("--changed-files-from", type=Path)
    scope.add_argument("--selector", help="manual all/name/path selector")
    parser.add_argument("--head", default="HEAD", help="head Git revision with --base")
    parser.add_argument("--github-output", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--expect-docs-only", action="store_true")
    parser.add_argument("--expect-no-example-builds", action="store_true")
    parser.add_argument("--strict-unknown", action="store_true")
    args = parser.parse_args()

    try:
        if args.selector is not None:
            report = classify_selector(args.selector)
        elif args.working_tree:
            report = classify_changes(working_tree_changes(REPOSITORY_ROOT))
        elif args.changed_files_from:
            lines = args.changed_files_from.read_text(encoding="utf-8").splitlines()
            report = classify_changes(parse_name_status(lines))
        else:
            report = classify_changes(
                git_range_changes(REPOSITORY_ROOT, args.base, args.head)
            )
    except (ClassificationError, OSError, ValueError) as exc:
        print(f"CI routing failed: {exc}", file=sys.stderr)
        return 2

    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.report:
        args.report.write_text(rendered + "\n", encoding="utf-8", newline="\n")
    if args.github_output:
        append_github_outputs(args.github_output, report)

    policy_errors: list[str] = []
    if args.expect_docs_only and not report["scope"]["docs_only"]:
        policy_errors.append("the change set is not documentation-only")
    if args.expect_no_example_builds and report["esp_idf"]["selected"]:
        policy_errors.append("the change set selects ESP-IDF example builds")
    if args.strict_unknown and report["unknown_paths"]:
        policy_errors.append("the change set contains unknown paths")
    for error in policy_errors:
        print(f"CI routing policy failed: {error}", file=sys.stderr)
    return 1 if policy_errors else 0


if __name__ == "__main__":
    sys.exit(main())
