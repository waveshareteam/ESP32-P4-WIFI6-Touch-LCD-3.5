#!/usr/bin/env python3
"""Validate first-party bilingual documentation and repository-local links."""

from __future__ import annotations

import html
import re
import sys
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
README_FILES = (ROOT / "README.md", ROOT / "README_ZH.md")
PRODUCT_IMAGE = ROOT / "assets" / "ESP32-P4-WIFI6-Touch-LCD-3.5-details-1.jpg"
EXAMPLE_ROOT = ROOT / "examples" / "esp-idf"
EXAMPLE_LINK_PREFIX = PurePosixPath("examples/esp-idf")
SUPPORTING_DOCS = (
    ROOT / "CONTRIBUTING.md",
    ROOT / "CONTRIBUTING_ZH.md",
    ROOT / "SUPPORT.md",
    ROOT / "SUPPORT_ZH.md",
    ROOT / "docs" / "ci.md",
    ROOT / "docs" / "ci_ZH.md",
    ROOT / "docs" / "components.md",
    ROOT / "docs" / "components_ZH.md",
    ROOT / "example" / "README.md",
    ROOT / "example" / "README_ZH.md",
    EXAMPLE_ROOT / "README.md",
    EXAMPLE_ROOT / "README_ZH.md",
)
DOCUMENT_COUNTERPARTS = {
    ROOT / "CONTRIBUTING.md": "CONTRIBUTING_ZH.md",
    ROOT / "CONTRIBUTING_ZH.md": "CONTRIBUTING.md",
    ROOT / "SUPPORT.md": "SUPPORT_ZH.md",
    ROOT / "SUPPORT_ZH.md": "SUPPORT.md",
    ROOT / "docs" / "ci.md": "ci_ZH.md",
    ROOT / "docs" / "ci_ZH.md": "ci.md",
    ROOT / "docs" / "components.md": "components_ZH.md",
    ROOT / "docs" / "components_ZH.md": "components.md",
    ROOT / "example" / "README.md": "README_ZH.md",
    ROOT / "example" / "README_ZH.md": "README.md",
    EXAMPLE_ROOT / "README.md": "README_ZH.md",
    EXAMPLE_ROOT / "README_ZH.md": "README.md",
}
REQUIRED_EXTERNAL_LINKS = {
    "README.md": (
        "https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.5.htm",
        "https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-3.5",
    ),
    "README_ZH.md": (
        "https://www.waveshare.net/shop/ESP32-P4-WIFI6-Touch-LCD-3.5.htm",
        "https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-3.5/",
    ),
}
MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)")
HTML_LINK_RE = re.compile(r"(?:href|src)=[\"']([^\"']+)[\"']", re.IGNORECASE)
LOCAL_PATH_RE = re.compile(
    r"(?:file://|(?<![A-Za-z0-9+.-])[A-Za-z]:[\\/]"
    r"|\\\\[^\\/\s]+[\\/][^\\/\s]+|/(?:Users|home|root)/)",
    re.IGNORECASE,
)


def add_error(errors: list[str], message: str) -> None:
    errors.append(message)


def extract_links(text: str) -> set[str]:
    links = {html.unescape(value).strip("<>") for value in MARKDOWN_LINK_RE.findall(text)}
    links.update(html.unescape(value) for value in HTML_LINK_RE.findall(text))
    return links


def check_example_inventory(errors: list[str]) -> tuple[str, ...]:
    try:
        discovered = {
            path.name
            for path in EXAMPLE_ROOT.iterdir()
            if path.is_dir() and (path / "CMakeLists.txt").is_file()
        }
    except OSError as exc:
        add_error(errors, f"cannot inspect ESP-IDF examples: {exc}")
        return ()

    if not discovered:
        add_error(errors, "no first-party ESP-IDF examples were found")
    return tuple(sorted(discovered))


def check_local_links(readme: Path, text: str, errors: list[str]) -> None:
    for raw_link in sorted(extract_links(text)):
        if raw_link.startswith(("#", "//")):
            continue
        parsed = urlsplit(raw_link)
        if parsed.scheme:
            continue

        relative = unquote(raw_link.split("#", 1)[0].split("?", 1)[0])
        if not relative:
            continue

        target = (readme.parent / relative).resolve()
        try:
            target.relative_to(ROOT)
        except ValueError:
            add_error(errors, f"{readme.name}: link escapes the repository: {raw_link}")
            continue

        if not target.exists():
            add_error(errors, f"{readme.name}: missing local link target: {raw_link}")


def documented_examples(text: str) -> set[str]:
    documented: set[str] = set()
    for raw_link in extract_links(text):
        parsed = urlsplit(raw_link)
        if parsed.scheme or raw_link.startswith(("#", "//")):
            continue
        relative = unquote(raw_link.split("#", 1)[0].split("?", 1)[0]).strip("/")
        path = PurePosixPath(relative)
        if len(path.parts) >= 3 and path.parts[:2] == EXAMPLE_LINK_PREFIX.parts:
            documented.add(path.parts[2])
    return documented


def check_readme(readme: Path, example_names: tuple[str, ...], errors: list[str]) -> None:
    try:
        raw = readme.read_bytes()
        text = raw.decode("utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        add_error(errors, f"{readme.name}: cannot read as UTF-8: {exc}")
        return

    if not raw.endswith(b"\n"):
        add_error(errors, f"{readme.name}: file must end with a newline")

    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.endswith((" ", "\t")):
            add_error(errors, f"{readme.name}:{line_number}: trailing whitespace")

    if LOCAL_PATH_RE.search(text):
        add_error(errors, f"{readme.name}: contains a host-local filesystem path")

    for required_link in REQUIRED_EXTERNAL_LINKS[readme.name]:
        if required_link not in text:
            add_error(errors, f"{readme.name}: missing required official link: {required_link}")

    counterpart = "README_ZH.md" if readme.name == "README.md" else "README.md"
    if counterpart not in text:
        add_error(errors, f"{readme.name}: missing language switch to {counterpart}")

    image_path = PRODUCT_IMAGE.relative_to(ROOT).as_posix()
    if image_path not in text:
        add_error(errors, f"{readme.name}: missing product image reference: {image_path}")

    for example_name in example_names:
        example_path = f"{EXAMPLE_LINK_PREFIX.as_posix()}/{example_name}/"
        if example_path not in text:
            add_error(errors, f"{readme.name}: missing example link: {example_path}")

    stale_examples = documented_examples(text) - set(example_names)
    for example_name in sorted(stale_examples):
        add_error(errors, f"{readme.name}: documents an unknown ESP-IDF example: {example_name}")

    check_local_links(readme, text, errors)


def check_supporting_document(
    document: Path, counterpart: str | None, errors: list[str]
) -> None:
    try:
        raw = document.read_bytes()
        text = raw.decode("utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        add_error(errors, f"{document.relative_to(ROOT)}: cannot read as UTF-8: {exc}")
        return

    display_name = document.relative_to(ROOT).as_posix()
    if not raw.endswith(b"\n"):
        add_error(errors, f"{display_name}: file must end with a newline")

    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.endswith((" ", "\t")):
            add_error(errors, f"{display_name}:{line_number}: trailing whitespace")

    if LOCAL_PATH_RE.search(text):
        add_error(errors, f"{display_name}: contains a host-local filesystem path")

    if counterpart is not None and counterpart not in text:
        add_error(errors, f"{display_name}: missing language switch to {counterpart}")

    check_local_links(document, text, errors)


def check_product_image(errors: list[str]) -> None:
    try:
        image = PRODUCT_IMAGE.read_bytes()
    except OSError as exc:
        add_error(errors, f"product image cannot be read: {exc}")
        return

    if len(image) < 10_000:
        add_error(errors, "product image is unexpectedly small")
    if not image.startswith(b"\xff\xd8\xff"):
        add_error(errors, "product image is not a JPEG file")


def main() -> int:
    errors: list[str] = []
    example_names = check_example_inventory(errors)
    for readme in README_FILES:
        check_readme(readme, example_names, errors)

    supporting_docs = list(SUPPORTING_DOCS)
    counterparts = dict(DOCUMENT_COUNTERPARTS)
    for example_name in example_names:
        example_root = EXAMPLE_ROOT / example_name
        english = example_root / "README.md"
        chinese = example_root / "README_ZH.md"
        supporting_docs.extend((english, chinese))
        counterparts[english] = "README_ZH.md"
        counterparts[chinese] = "README.md"

    for document in supporting_docs:
        check_supporting_document(document, counterparts.get(document), errors)
    check_product_image(errors)

    if errors:
        print("Documentation validation failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Documentation validation passed for first-party bilingual docs and local links.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
