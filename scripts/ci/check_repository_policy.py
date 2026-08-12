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
PROFILE_CONFIG = ROOT / "config" / "revision-profiles.json"
EXAMPLE12_MANIFEST = EXAMPLES / "12_esp32-p4-eye" / "main" / "idf_component.yml"
EXAMPLE12_IMAGES = EXAMPLES / "12_esp32-p4-eye" / "main" / "ui" / "images"
EXAMPLE12_CMAKE = EXAMPLES / "12_esp32-p4-eye" / "CMakeLists.txt"
EXAMPLE12_LVGL8_MANAGED_BSP_SHIM = EXAMPLES / "12_esp32-p4-eye" / "compat" / "lvgl8_managed_bsp.h"
EXAMPLE12_LVGL8_MANAGED_BSP_COMPONENTS = (
    "waveshare__esp32_p4_wifi6_touch_lcd_3_5",
    "bsp_extra",
    "main",
)


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


def check_arduino(policy: dict[str, object]) -> list[str]:
    arduino = policy["arduino"]
    expected = arduino["expected_sketch_count"]
    sketches = list((ROOT / "examples" / "arduino").glob("**/*.ino")) if (ROOT / "examples" / "arduino").exists() else []
    errors = []
    if len(sketches) != expected:
        errors.append(f"Arduino sketch count is {len(sketches)}, expected {expected}")
    if arduino["default_chip_variant"] != "prev3":
        errors.append("Arduino default ChipVariant must be prev3")
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
    if policy["default_profile"] != "rev1_3":
        errors.append("default revision profile must be rev1_3")
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


def check_example12_lvgl8_managed_bsp_shim(
    cmake_path: Path = EXAMPLE12_CMAKE,
    shim_path: Path = EXAMPLE12_LVGL8_MANAGED_BSP_SHIM,
) -> list[str]:
    errors: list[str] = []
    if not shim_path.is_file():
        return ["Example 12 LVGL 8 managed BSP compatibility shim is missing"]

    shim = shim_path.read_text(encoding="utf-8")
    required_shim = (
        '#include "lvgl.h"',
        "#if LVGL_VERSION_MAJOR == 8",
        "typedef lv_disp_t lv_display_t;",
        "typedef lv_disp_rot_t lv_disp_rotation_t;",
        "#elif LVGL_VERSION_MAJOR == 9",
        "#error",
    )
    if any(token not in shim for token in required_shim):
        errors.append("Example 12 LVGL 8 shim must guard the required LVGL type aliases")

    cmake = cmake_path.read_text(encoding="utf-8")
    targets = re.search(
        r"(?ms)^set\(lvgl8_managed_bsp_components\s*(?P<targets>.*?)^\)", cmake
    )
    configured_components = (
        tuple(re.findall(r"(?m)^\s*([A-Za-z0-9_]+)\s*$", targets.group("targets")))
        if targets
        else ()
    )
    if configured_components != EXAMPLE12_LVGL8_MANAGED_BSP_COMPONENTS:
        errors.append("Example 12 must list exactly the managed BSP, bsp_extra, and main LVGL 8 shim consumers")

    required_loop = (
        "foreach(lvgl8_managed_bsp_component IN LISTS lvgl8_managed_bsp_components)",
        'idf_component_get_property(lvgl8_managed_bsp_target\n        "${lvgl8_managed_bsp_component}" COMPONENT_LIB)',
        'NOT TARGET "${lvgl8_managed_bsp_target}"',
        'target_compile_options("${lvgl8_managed_bsp_target}" PRIVATE\n        "SHELL:-include \\"${lvgl8_managed_bsp_shim}\\"")',
        "endforeach()",
    )
    if any(token not in cmake for token in required_loop):
        errors.append("Example 12 must privately force-include the shim on exactly its three direct managed BSP consumers")
    if any(token in cmake for token in ("add_compile_options(", "include_directories(", "target_compile_options(PUBLIC", "target_compile_options(INTERFACE")):
        errors.append("Example 12 LVGL 8 managed BSP shim must not use public or global compile settings")
    return errors


def check_workflows() -> list[str]:
    errors = []
    examples = (ROOT / ".github" / "workflows" / "esp-idf.yml").read_text(encoding="utf-8")
    product = (ROOT / ".github" / "workflows" / "product-firmware.yml").read_text(encoding="utf-8")
    arduino = (ROOT / ".github" / "workflows" / "arduino-policy.yml").read_text(encoding="utf-8")
    docs = (ROOT / ".github" / "workflows" / "docs.yml").read_text(encoding="utf-8")
    repository_policy = (ROOT / ".github" / "workflows" / "repository-policy.yml").read_text(encoding="utf-8")
    exact_head = "${{ github.event.pull_request.head.sha || github.sha }}"
    required_example = ("v5.5.5", "v6.0.2", "-B build-rev1_3 -D SDKCONFIG=sdkconfig.rev1_3", "--profile rev1_3", "--build-dir build-rev1_3", "esp32p4-rev1_3-")
    if any(token not in examples for token in required_example):
        errors.append("ESP-IDF example workflow is missing the rev1_3 12x2 contract")
    if examples.count(f"ref: {exact_head}") < 2 or examples.count(exact_head) < 5 or "--git-sha" not in examples:
        errors.append("ESP-IDF workflow must check out and package the exact final head SHA")
    required_product = ("examples/esp-idf/12_esp32-p4-eye", "v6.0.2", "-B build-${{ matrix.profile }} -D SDKCONFIG=sdkconfig.${{ matrix.profile }}", "rev1_3", "rev3_x", "retention-days: 14")
    if any(token not in product for token in required_product):
        errors.append("product firmware workflow is missing the two-profile artifact contract")
    if product.count(f"ref: {exact_head}") < 2 or product.count(exact_head) < 5 or "--git-sha" not in product:
        errors.append("product firmware workflow must check out and package the exact final head SHA")
    if "check_repository_policy.py --arduino-only" not in arduino or "compile" in arduino.casefold():
        errors.append("Arduino policy workflow must verify inventory only, without a compile claim")
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
        errors.extend(check_example12_lvgl_contract())
        errors.extend(check_example12_lvgl8_managed_bsp_shim())
        errors.extend(check_workflows())
    for error in errors:
        print(f"policy: {error}", file=sys.stderr)
    if errors:
        return 1
    print("repository policy OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
