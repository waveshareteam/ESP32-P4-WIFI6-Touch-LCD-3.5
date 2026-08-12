from __future__ import annotations

import json
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
ROOT = SCRIPT_DIR.parents[1]
POLICY_SPEC = importlib.util.spec_from_file_location(
    "check_repository_policy", SCRIPT_DIR / "check_repository_policy.py"
)
assert POLICY_SPEC and POLICY_SPEC.loader
POLICY = importlib.util.module_from_spec(POLICY_SPEC)
POLICY_SPEC.loader.exec_module(POLICY)


def parse_sdkconfig(path: Path) -> dict[str, str]:
    return dict(
        line.split("=", 1)
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.startswith("CONFIG_") and "=" in line
    )


class RevisionProfileTests(unittest.TestCase):
    def setUp(self) -> None:
        self.policy = json.loads((ROOT / "config" / "revision-profiles.json").read_text(encoding="utf-8"))
        self.projects = sorted((ROOT / "examples" / "esp-idf").glob("*/CMakeLists.txt"))

    def test_all_projects_include_central_profile_and_exact_defaults(self) -> None:
        self.assertEqual(len(self.projects), 12)
        self.assertEqual(self.policy["default_profile"], "rev1_3")
        for cmake_path in self.projects:
            with self.subTest(project=cmake_path.parent.name):
                cmake = cmake_path.read_text(encoding="utf-8")
                self.assertIn("config/revision_profiles.cmake", cmake)
                self.assertIn("waveshare_configure_revision_profile", cmake)
                for profile, expected in self.policy["profiles"].items():
                    self.assertEqual(parse_sdkconfig(cmake_path.parent / f"sdkconfig.defaults.{profile}"), expected)

    def test_central_policy_encodes_arduino_zero_inventory_prev3(self) -> None:
        self.assertEqual(self.policy["arduino"], {"expected_sketch_count": 0, "default_chip_variant": "prev3"})

    def test_repository_policy_checker(self) -> None:
        completed = subprocess.run([sys.executable, str(SCRIPT_DIR / "check_repository_policy.py")], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_example12_lvgl_policy_rejects_direct_port_and_invalid_assets(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "idf_component.yml"
            manifest.write_text(
                "dependencies:\n"
                "  lvgl/lvgl:\n"
                "    version: \"8.3.*\"\n"
                "    public: true\n"
                "  espressif/esp_lvgl_port:\n"
                "    version: \"2.8.0~1\"\n",
                encoding="utf-8",
            )
            images = root / "images"
            images.mkdir()
            (images / "ui_img_test.c").write_text(
                "lv_img_dsc_t LV_IMG_CF always_zero", encoding="utf-8"
            )
            errors = POLICY.check_example12_lvgl_contract(manifest, images)

        self.assertTrue(any("must not directly depend" in error for error in errors))
        self.assertTrue(any("must retain 21" in error for error in errors))

    def test_example12_lvgl8_managed_bsp_shim_policy_rejects_leaks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cmake = root / "CMakeLists.txt"
            shim = root / "lvgl8_managed_bsp.h"
            cmake.write_text(
                "idf_component_get_property(lvgl8_managed_bsp_target\n"
                "    wrong__bsp COMPONENT_LIB)\n"
                "add_compile_options(-include broken.h)\n"
                "target_compile_options(\"${lvgl8_managed_bsp_target}\" PUBLIC\n"
                "    \"SHELL:-include \\\"${lvgl8_managed_bsp_shim}\\\"\")\n",
                encoding="utf-8",
            )
            shim.write_text(
                '#include "lvgl.h"\ntypedef lv_disp_t lv_display_t;\n', encoding="utf-8"
            )
            errors = POLICY.check_example12_lvgl8_managed_bsp_shim(cmake, shim)

        self.assertTrue(any("must guard" in error for error in errors))
        self.assertTrue(any("exact managed BSP target" in error for error in errors))
        self.assertTrue(any("public or global" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
