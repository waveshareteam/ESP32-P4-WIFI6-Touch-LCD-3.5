#!/usr/bin/env python3
"""Deterministic repository policy checks for revision-profile CI contracts."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXAMPLES = ROOT / "examples" / "esp-idf"
ARDUINO_EXAMPLES = ROOT / "examples" / "arduino" / "examples"
PROFILE_CONFIG = ROOT / "config" / "revision-profiles.json"
EXAMPLE10_MANIFEST = EXAMPLES / "10_mp4_player" / "main" / "idf_component.yml"
EXAMPLE09_VIDEO_HEADER = EXAMPLES / "09_video_lcd_display" / "main" / "app_video.h"
EXAMPLE09_DEFAULTS = EXAMPLES / "09_video_lcd_display" / "sdkconfig.defaults"
EXAMPLE10_MAIN = EXAMPLES / "10_mp4_player" / "main" / "main.c"
EXAMPLE10_DEFAULTS = EXAMPLES / "10_mp4_player" / "sdkconfig.defaults"
EXAMPLE12_MANIFEST = EXAMPLES / "12_esp32-p4-eye" / "main" / "idf_component.yml"
EXAMPLE12_IMAGES = EXAMPLES / "12_esp32-p4-eye" / "main" / "ui" / "images"
EXAMPLE12_CAMERA_KCONFIG = EXAMPLES / "12_esp32-p4-eye" / "main" / "Kconfig.projbuild"
ARDUINO_BOARD_HEADER = ROOT / "examples" / "arduino" / "libraries" / "Waveshare_LCD35" / "src" / "lcd35_board.h"
ARDUINO_CAMERA_SKETCHES = (
    ARDUINO_EXAMPLES / "06_Camera_Preview" / "06_Camera_Preview.ino",
    ARDUINO_EXAMPLES / "07_Camera_ISP_Tuning" / "07_Camera_ISP_Tuning.ino",
)
REMOVED_MANAGED_BSP_DISPLAY_SYMBOLS = (
    "CONFIG_BSP_LCD_" + "COLOR_FORMAT",
    "CONFIG_BSP_LCD_" + "DPI_BUFFER_NUMS",
)
FIRST_PARTY_ESP_IDF_SOURCE_CONFIG_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hpp",
    ".cmake",
    ".conf",
    ".defaults",
    ".json",
    ".txt",
    ".yml",
    ".yaml",
}


def load_policy() -> dict[str, object]:
    return json.loads(PROFILE_CONFIG.read_text(encoding="utf-8"))


def project_roots() -> list[Path]:
    return sorted(path.parent for path in EXAMPLES.glob("*/CMakeLists.txt"))


def parse_sdkconfig(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value.strip().strip('"')
    return values


def parse_kconfig_defaults(path: Path) -> dict[str, str]:
    defaults: dict[str, str] = {}
    current_symbol: str | None = None
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped.startswith("config "):
            current_symbol = stripped.split(maxsplit=1)[1]
        elif current_symbol and stripped.startswith("default "):
            defaults[current_symbol] = stripped.split(maxsplit=1)[1]
    return defaults


def check_arduino(policy: dict[str, object]) -> list[str]:
    arduino = policy["arduino"]
    expected = arduino["expected_sketch_count"]
    sketches = sorted(ARDUINO_EXAMPLES.glob("*/*.ino")) if ARDUINO_EXAMPLES.is_dir() else []
    errors: list[str] = []
    if len(sketches) != expected:
        errors.append(f"Arduino sketch count is {len(sketches)}, expected {expected}")
    for sketch in sketches:
        if sketch.stem != sketch.parent.name:
            errors.append(f"Arduino sketch must match its directory name: {sketch.relative_to(ROOT)}")
    if arduino["default_chip_variant"] != "postv3":
        errors.append("Arduino default ChipVariant must be postv3")
    expected_fqbn = (
        "esp32:esp32:esp32p4:ChipVariant=postv3,PSRAM=enabled,FlashSize=16M,"
        "FlashMode=qio,FlashFreq=80,PartitionScheme=app3M_fat9M_16MB,"
        "UploadMode=default,UploadSpeed=921600"
    )
    if arduino.get("default_fqbn") != expected_fqbn:
        errors.append("Arduino default FQBN must select postv3, 16M QIO80 flash, PSRAM, and app3M_fat9M_16MB")
    return errors


def check_profiles(policy: dict[str, object]) -> list[str]:
    profiles = policy["profiles"]
    roots = project_roots()
    errors: list[str] = []
    if len(roots) != 12:
        errors.append(f"expected 12 ESP-IDF roots, found {len(roots)}")
    for project in roots:
        cmake = (project / "CMakeLists.txt").read_text(encoding="utf-8")
        if "config/revision_profiles.cmake" not in cmake or "waveshare_configure_revision_profile" not in cmake:
            errors.append(f"{project.relative_to(ROOT)} does not include the central revision profile")
        for profile, expected in profiles.items():
            values = parse_sdkconfig(project / f"sdkconfig.defaults.{profile}")
            if values != expected:
                errors.append(f"{project.relative_to(ROOT)}/sdkconfig.defaults.{profile} does not exactly match policy")
    if policy["default_profile"] != "rev3_x":
        errors.append("default revision profile must be rev3_x")
    profile_cmake = (ROOT / "config" / "revision_profiles.cmake").read_text(encoding="utf-8")
    if 'set(WAVESHARE_REVISION_PROFILE "rev3_x" CACHE STRING' not in profile_cmake:
        errors.append("central CMake revision profile default must be rev3_x")
    return errors


def check_bsp(policy: dict[str, object]) -> list[str]:
    bsp = policy["official_bsp"]
    local_name = bsp["local_directory"]
    # Git does not preserve empty directories; ignore empty remnants of a pending
    # migration deletion while rejecting any checked-in local BSP source content.
    local_dirs = [
        path for path in EXAMPLES.glob(f"**/{local_name}")
        if path.is_dir() and any(candidate.is_file() for candidate in path.rglob("*"))
    ]
    errors = []
    if local_dirs:
        errors.append("obsolete local official BSP directories remain: " + ", ".join(str(path.relative_to(ROOT)) for path in local_dirs))
    dependency = re.escape(bsp["dependency"])
    version = re.escape(bsp["version"])
    pattern = re.compile(dependency + r"\s*:\s*\n\s*version:\s*[\"']?" + version)
    manifests = list(EXAMPLES.glob("**/idf_component.yml"))
    matching = [path for path in manifests if pattern.search(path.read_text(encoding="utf-8"))]
    if not matching:
        errors.append("official BSP dependency/version is not present in any manifest")
    for path in manifests:
        text = path.read_text(encoding="utf-8")
        if bsp["dependency"] in text and not pattern.search(text):
            errors.append(f"{path.relative_to(ROOT)} does not pin the official BSP to {bsp['version']}")
    return errors


def manifest_dependency_block(text: str, dependency: str) -> str | None:
    match = re.search(
        rf"(?ms)^  {re.escape(dependency)}:\n(?P<body>.*?)(?=^  \S|\Z)", text
    )
    return match.group("body") if match else None


def check_example10_audio_codec_contract(manifest_path: Path = EXAMPLE10_MANIFEST) -> list[str]:
    manifest = manifest_path.read_text(encoding="utf-8")
    audio_codec = manifest_dependency_block(manifest, "espressif/esp_audio_codec")
    if audio_codec is None or not re.search(
        r'^    version:\s*">=2\.3\.0,<2\.6\.0"\s*$', audio_codec, re.MULTILINE
    ):
        return ["Example 10 must keep espressif/esp_audio_codec at >=2.3.0,<2.6.0 for explicit rev1_3 compatibility"]
    return []


def check_display_config_contract(
    example09_header: Path = EXAMPLE09_VIDEO_HEADER,
    example09_defaults: Path = EXAMPLE09_DEFAULTS,
    example10_main: Path = EXAMPLE10_MAIN,
    example10_defaults: Path = EXAMPLE10_DEFAULTS,
) -> list[str]:
    errors: list[str] = []
    example09_header_text = example09_header.read_text(encoding="utf-8")
    example10_main_text = example10_main.read_text(encoding="utf-8")
    if not re.search(r"^#define APP_VIDEO_FMT\s+\(APP_VIDEO_FMT_RGB565\)\s*$", example09_header_text, re.MULTILINE):
        errors.append("Example 09 must unconditionally set APP_VIDEO_FMT to APP_VIDEO_FMT_RGB565")
    if not re.search(r"^#define APP_LCD_BUFFER_COUNT\s+2\s*$", example10_main_text, re.MULTILINE):
        errors.append("Example 10 must define APP_LCD_BUFFER_COUNT as 2")
    required_uses = (
        "static void *lcd_buffer[APP_LCD_BUFFER_COUNT];",
        "buffer_index < APP_LCD_BUFFER_COUNT",
        ".buffer_count = APP_LCD_BUFFER_COUNT,",
    )
    if any(token not in example10_main_text for token in required_uses):
        errors.append("Example 10 must use APP_LCD_BUFFER_COUNT for every display-buffer callsite")
    return errors


def check_camera_sccb_pin_contract(
    example12_kconfig: Path = EXAMPLE12_CAMERA_KCONFIG,
    example09_defaults: Path = EXAMPLE09_DEFAULTS,
    arduino_board_header: Path = ARDUINO_BOARD_HEADER,
    arduino_camera_sketches: tuple[Path, ...] = ARDUINO_CAMERA_SKETCHES,
) -> list[str]:
    errors: list[str] = []
    expected = {"SCL": "8", "SDA": "7"}
    kconfig_defaults = parse_kconfig_defaults(example12_kconfig)
    example09_values = parse_sdkconfig(example09_defaults)
    board_header = arduino_board_header.read_text(encoding="utf-8")

    for signal, gpio in expected.items():
        kconfig_symbol = f"EXAMPLE_MIPI_CSI_SCCB_I2C_{signal}_PIN"
        if kconfig_defaults.get(kconfig_symbol) != gpio:
            errors.append(f"Example 12 camera {signal} fallback must default to GPIO {gpio}")
        if example09_values.get(f"CONFIG_{kconfig_symbol}") != gpio:
            errors.append(f"Example 09 camera {signal} must use GPIO {gpio}")
        if not re.search(rf"kI2c{signal.title()}\s*=\s*{gpio}\s*;", board_header):
            errors.append(f"Arduino board I2C {signal} must use GPIO {gpio}")
        for sketch in arduino_camera_sketches:
            text = sketch.read_text(encoding="utf-8")
            if not re.search(rf"^#define CAMERA_SCCB_{signal}\s+{gpio}\s*$", text, re.MULTILINE):
                errors.append(f"{sketch.relative_to(ROOT)} camera {signal} must use GPIO {gpio}")
    return errors


def is_first_party_esp_idf_source_or_config(path: Path) -> bool:
    return (
        path.suffix in FIRST_PARTY_ESP_IDF_SOURCE_CONFIG_SUFFIXES
        or path.name.startswith("sdkconfig.defaults")
        or path.name.startswith("Kconfig")
    )


def check_removed_managed_bsp_display_symbols(examples: Path = EXAMPLES) -> list[str]:
    errors: list[str] = []
    for path in sorted(examples.rglob("*")):
        relative_parts = path.relative_to(examples).parts
        if any(part == "managed_components" or part.startswith("build") for part in relative_parts):
            continue
        if not path.is_file() or not is_first_party_esp_idf_source_or_config(path):
            continue
        text = path.read_text(encoding="utf-8")
        if any(symbol in text for symbol in REMOVED_MANAGED_BSP_DISPLAY_SYMBOLS):
            display_name = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
            errors.append(f"{display_name} retains removed BSP display Kconfig symbols")
    return errors


def check_example12_lvgl_contract(
    manifest_path: Path = EXAMPLE12_MANIFEST, image_dir: Path = EXAMPLE12_IMAGES
) -> list[str]:
    errors: list[str] = []
    manifest = manifest_path.read_text(encoding="utf-8")
    lvgl = manifest_dependency_block(manifest, "lvgl/lvgl")
    if lvgl is None:
        errors.append("Example 12 must directly depend on lvgl/lvgl 8.3.*")
    else:
        if not re.search(r'^    version:\s*["\']?8\.3\.\*["\']?\s*$', lvgl, re.MULTILINE):
            errors.append("Example 12 must pin lvgl/lvgl to 8.3.*")
        if not re.search(r"^    public:\s*true\s*$", lvgl, re.MULTILINE):
            errors.append("Example 12 lvgl/lvgl dependency must be public")
    if manifest_dependency_block(manifest, "espressif/esp_lvgl_port") is not None:
        errors.append("Example 12 must not directly depend on espressif/esp_lvgl_port")

    images = sorted(image_dir.glob("*.c"))
    if len(images) != 21:
        errors.append(f"Example 12 must retain 21 SquareLine image C files, found {len(images)}")
    for image in images:
        text = image.read_text(encoding="utf-8")
        if not all(marker in text for marker in ("lv_img_dsc_t", "LV_IMG_CF", "always_zero")):
            errors.append(f"{image.relative_to(ROOT)} does not retain the LVGL 8 image contract")
    return errors


def check_workflows() -> list[str]:
    errors = []
    examples = (ROOT / ".github" / "workflows" / "esp-idf.yml").read_text(encoding="utf-8")
    product = (ROOT / ".github" / "workflows" / "product-firmware.yml").read_text(encoding="utf-8")
    arduino = (ROOT / ".github" / "workflows" / "arduino-policy.yml").read_text(encoding="utf-8")
    docs = (ROOT / ".github" / "workflows" / "docs.yml").read_text(encoding="utf-8")
    repository_policy = (ROOT / ".github" / "workflows" / "repository-policy.yml").read_text(encoding="utf-8")
    exact_head = "${{ github.event.pull_request.head.sha || github.sha }}"
    required_example = ("v5.5.5", "v6.0.2", "-B build-rev3_x -D SDKCONFIG=sdkconfig.rev3_x", "--profile rev3_x", "--build-dir build-rev3_x", "esp32p4-rev3_x-")
    if any(token not in examples for token in required_example):
        errors.append("ESP-IDF example workflow is missing the rev3_x 12x2 contract")
    if examples.count(f"ref: {exact_head}") < 2 or examples.count(exact_head) < 5 or "--git-sha" not in examples:
        errors.append("ESP-IDF workflow must check out and package the exact final head SHA")
    required_product = ("examples/esp-idf/12_esp32-p4-eye", "v6.0.2", "-B build-${{ matrix.profile }} -D SDKCONFIG=sdkconfig.${{ matrix.profile }}", "rev1_3", "rev3_x", "retention-days: 14")
    if any(token not in product for token in required_product):
        errors.append("product firmware workflow is missing the two-profile artifact contract")
    if product.count(f"ref: {exact_head}") < 2 or product.count(exact_head) < 5 or "--git-sha" not in product:
        errors.append("product firmware workflow must check out and package the exact final head SHA")
    required_arduino = (
        "ARDUINO_CORE_VERSION: \"3.3.11\"",
        "version: \"1.5.1\"",
        "ChipVariant=postv3,PSRAM=enabled,FlashSize=16M,FlashMode=qio,FlashFreq=80,PartitionScheme=app3M_fat9M_16MB,UploadMode=default,UploadSpeed=921600",
        "scripts/discover_arduino_examples.py",
        "arduino-cli compile",
        "GFX Library for Arduino@1.6.7",
        "lvgl@9.3.0",
        "Arduino build matrix",
        "arduino_build_required",
    )
    if any(token not in arduino for token in required_arduino):
        errors.append("Arduino workflow is missing the published-library 10-sketch compile contract")
    if "Package firmware" in arduino or "upload-artifact" in arduino.casefold():
        errors.append("Arduino workflow must compile examples only and must not package firmware")
    if f"ref: {exact_head}" not in docs or f"ref: {exact_head}" not in arduino or repository_policy.count(f"ref: {exact_head}") < 2:
        errors.append("final-SHA validation workflows must explicitly check out the exact head")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-only", action="store_true")
    args = parser.parse_args()
    policy = load_policy()
    errors = check_arduino(policy)
    if not args.arduino_only:
        errors.extend(check_profiles(policy))
        errors.extend(check_bsp(policy))
        errors.extend(check_example10_audio_codec_contract())
        errors.extend(check_display_config_contract())
        errors.extend(check_camera_sccb_pin_contract())
        errors.extend(check_removed_managed_bsp_display_symbols())
        errors.extend(check_example12_lvgl_contract())
        errors.extend(check_workflows())
    for error in errors:
        print(f"policy: {error}", file=sys.stderr)
    if errors:
        return 1
    print("repository policy OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
