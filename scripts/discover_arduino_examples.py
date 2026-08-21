#!/usr/bin/env python3
"""Discover the first-party Arduino sketch directories used by CI."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ARDUINO_EXAMPLES = ROOT / "examples" / "arduino" / "examples"


def selector_matches(name: str, path: str, selector: str) -> bool:
    """Return whether a comma-separated manual selector includes a sketch."""
    if not selector or selector == "all":
        return True
    selectors = [item.strip() for item in selector.split(",") if item.strip()]
    return any(
        item == name or item == path or path.startswith(item.rstrip("/") + "/")
        for item in selectors
    )


def discover() -> list[dict[str, str]]:
    """Return one entry for each correctly named top-level sketch directory."""
    if not ARDUINO_EXAMPLES.is_dir():
        return []

    sketches: list[dict[str, str]] = []
    for project in sorted(ARDUINO_EXAMPLES.iterdir(), key=lambda item: item.name.casefold()):
        if not project.is_dir():
            continue
        ino_files = sorted(project.glob("*.ino"))
        if len(ino_files) != 1 or ino_files[0].stem != project.name:
            raise ValueError(
                f"{project.relative_to(ROOT)} must contain exactly one sketch named "
                f"{project.name}.ino"
            )
        sketches.append(
            {
                "name": project.name,
                "path": project.relative_to(ROOT).as_posix(),
                "ino": ino_files[0].name,
            }
        )
    return sketches


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selector", default="all")
    parser.add_argument("--core", required=True)
    parser.add_argument("--fqbn", required=True)
    parser.add_argument("--github-output", required=True, type=Path)
    args = parser.parse_args()

    entries = [
        entry | {"core": args.core, "fqbn": args.fqbn}
        for entry in discover()
        if selector_matches(entry["name"], entry["path"], args.selector)
    ]
    if args.selector and args.selector != "all" and not entries:
        raise ValueError(f"selector matched no Arduino sketch: {args.selector}")
    matrix = {"include": entries}
    with args.github_output.open("a", encoding="utf-8") as output:
        output.write(f"matrix={json.dumps(matrix, separators=(',', ':'))}\n")
        output.write(f"count={len(entries)}\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as error:
        raise SystemExit(f"discovery: {error}")
