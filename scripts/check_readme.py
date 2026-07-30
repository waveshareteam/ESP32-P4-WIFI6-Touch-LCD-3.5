#!/usr/bin/env python3
"""Validate the repository's bilingual top-level documentation."""

from __future__ import annotations

import html
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
README_FILES = (ROOT / "README.md", ROOT / "README_ZH.md")
PRODUCT_IMAGE = ROOT / "assets" / "ESP32-P4-WIFI6-Touch-LCD-3.5-details-1.jpg"
EXAMPLE_ROOT = ROOT / "example" / "ESP-IDF"
EXAMPLE_NAMES = (
    "01_HowToCreateProject",
    "02_HelloWorld",
    "03_i2c_tools",
    "04_wifistation",
    "05_sdmmc",
    "06_I2SCodec",
    "07_Displaycolorbar",
    "08_lvgl_demo_v9",
    "09_video_lcd_display",
    "10_mp4_player",
    "11_esp_brookesia_phone",
    "12_esp32-p4-eye",
)
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


def check_example_inventory(errors: list[str]) -> None:
    try:
        discovered = {
            path.name
            for path in EXAMPLE_ROOT.iterdir()
            if path.is_dir() and (path / "CMakeLists.txt").is_file()
        }
    except OSError as exc:
        add_error(errors, f"cannot inspect ESP-IDF examples: {exc}")
        return

    expected = set(EXAMPLE_NAMES)
    for name in sorted(expected - discovered):
        add_error(errors, f"documented ESP-IDF example is missing from the repository: {name}")
    for name in sorted(discovered - expected):
        add_error(errors, f"ESP-IDF example is missing from the bilingual README inventory: {name}")


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


def check_readme(readme: Path, errors: list[str]) -> None:
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

    for example_name in EXAMPLE_NAMES:
        example_path = f"example/ESP-IDF/{example_name}/"
        if example_path not in text:
            add_error(errors, f"{readme.name}: missing example link: {example_path}")

    check_local_links(readme, text, errors)


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
    check_example_inventory(errors)
    for readme in README_FILES:
        check_readme(readme, errors)
    check_product_image(errors)

    if errors:
        print("Documentation validation failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Documentation validation passed for README.md and README_ZH.md.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
