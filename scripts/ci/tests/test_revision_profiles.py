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
        self.assertEqual(self.policy["default_profile"], "rev3_x")
        profile_cmake = (ROOT / "config" / "revision_profiles.cmake").read_text(encoding="utf-8")
        self.assertIn('set(WAVESHARE_REVISION_PROFILE "rev3_x" CACHE STRING', profile_cmake)
        for cmake_path in self.projects:
            with self.subTest(project=cmake_path.parent.name):
                cmake = cmake_path.read_text(encoding="utf-8")
                self.assertIn("config/revision_profiles.cmake", cmake)
                self.assertIn("waveshare_configure_revision_profile", cmake)
                for profile, expected in self.policy["profiles"].items():
                    self.assertEqual(parse_sdkconfig(cmake_path.parent / f"sdkconfig.defaults.{profile}"), expected)

    def test_central_policy_encodes_ten_postv3_arduino_sketches(self) -> None:
        self.assertEqual(
            self.policy["arduino"],
            {
                "expected_sketch_count": 10,
                "default_chip_variant": "postv3",
                "default_fqbn": (
                    "esp32:esp32:esp32p4:ChipVariant=postv3,PSRAM=enabled,FlashSize=16M,"
                    "FlashMode=qio,FlashFreq=80,PartitionScheme=app3M_fat9M_16MB,"
                    "UploadMode=default,UploadSpeed=921600"
                ),
            },
        )

    def test_lcd5_arduino_library_baseline_is_bundled(self) -> None:
        libraries = ROOT / "examples" / "arduino" / "libraries"
        expected_files = (
            "GFX_Library_for_Arduino/src/Arduino_GFX_Library.h",
            "GFX_Library_for_Arduino/src/databus/Arduino_ESP32DSIPanel.cpp",
            "lvgl/src/demos/lv_demos.h",
            "lvgl/src/demos/widgets/lv_demo_widgets.c",
            "displays/gt911.cpp",
            "displays/touch.cpp",
            "lv_conf.h",
        )
        for relative_path in expected_files:
            with self.subTest(path=relative_path):
                self.assertTrue((libraries / relative_path).is_file())
        self.assertIn(
            "version=1.6.0",
            (libraries / "GFX_Library_for_Arduino" / "library.properties").read_text(encoding="utf-8").splitlines(),
        )
        self.assertIn(
            "version=9.3.0",
            (libraries / "lvgl" / "library.properties").read_text(encoding="utf-8").splitlines(),
        )

    def test_flasher_rejects_unsupported_silicon_revision_gaps(self) -> None:
        flasher = (ROOT / "scripts" / "Flash-CI-Firmware.ps1").read_text(encoding="utf-8")
        self.assertIn("if ($Major -eq 1) { return 'rev1_3' }", flasher)
        self.assertIn("if ($Major -eq 3) { return 'rev3_x' }", flasher)
        self.assertIn("supported ranges are [1.0, 2.0) and [3.0, 4.0)", flasher)
        for revision in ("Major = 0; Minor = 9", "Major = 2; Minor = 0", "Major = 4; Minor = 0"):
            self.assertIn(revision, flasher)

    def test_repository_policy_checker(self) -> None:
        completed = subprocess.run([sys.executable, str(SCRIPT_DIR / "check_repository_policy.py")], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_example10_audio_codec_policy_rejects_v2_6_compatible_range(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / "idf_component.yml"
            manifest.write_text(
                "dependencies:\n"
                "  espressif/esp_audio_codec:\n"
                '    version: "^2.3.0"\n'
                "    public: true\n",
                encoding="utf-8",
            )
            errors = POLICY.check_example10_audio_codec_contract(manifest)

        self.assertTrue(any(">=2.3.0,<2.6.0" in error for error in errors))

    def test_display_config_policy_rejects_removed_bsp_symbols(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stale_defaults = root / "11_any_first_party_example" / "sdkconfig.defaults"
            stale_defaults.parent.mkdir()
            stale_defaults.write_text("CONFIG_BSP_LCD_" "DPI_BUFFER_NUMS=3\n", encoding="utf-8")
            errors = POLICY.check_removed_managed_bsp_display_symbols(root)

        self.assertTrue(any("retains removed BSP display Kconfig symbols" in error for error in errors))

    def test_display_config_policy_skips_generated_idf_trees(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for generated_name in ("build-rev3_x", "managed_components"):
                generated_header = root / "07_Displaycolorbar" / generated_name / "dependency" / "invalid.h"
                generated_header.parent.mkdir(parents=True, exist_ok=True)
                generated_header.write_bytes(b"\xfc")

            errors = POLICY.check_removed_managed_bsp_display_symbols(root)

        self.assertEqual(errors, [])

    def test_display_config_policy_rejects_incomplete_app_buffer_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            example09_header = root / "app_video.h"
            example09_defaults = root / "example09.defaults"
            example10_main = root / "main.c"
            example10_defaults = root / "example10.defaults"
            example09_header.write_text("#define APP_VIDEO_FMT (APP_VIDEO_FMT_RGB888)\n", encoding="utf-8")
            example09_defaults.write_text("", encoding="utf-8")
            example10_main.write_text("#define APP_LCD_BUFFER_COUNT 3\n", encoding="utf-8")
            example10_defaults.write_text("", encoding="utf-8")
            errors = POLICY.check_display_config_contract(
                example09_header, example09_defaults, example10_main, example10_defaults
            )

        self.assertTrue(any("APP_VIDEO_FMT" in error for error in errors))
        self.assertTrue(any("APP_LCD_BUFFER_COUNT as 2" in error for error in errors))
        self.assertTrue(any("every display-buffer callsite" in error for error in errors))

    def test_camera_sccb_policy_rejects_wrong_fallback_pins(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            kconfig = root / "Kconfig.projbuild"
            defaults = root / "sdkconfig.defaults"
            board_header = root / "lcd35_board.h"
            camera_sketches = (root / "06.ino", root / "07.ino")
            kconfig.write_text(
                "config EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN\n    default 34\n"
                "config EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN\n    default 31\n",
                encoding="utf-8",
            )
            defaults.write_text(
                "CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN=8\n"
                "CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN=7\n",
                encoding="utf-8",
            )
            board_header.write_text(
                "inline constexpr int kI2cScl = 8;\ninline constexpr int kI2cSda = 7;\n",
                encoding="utf-8",
            )
            for sketch in camera_sketches:
                sketch.write_text(
                    "#define CAMERA_SCCB_SCL  8\n#define CAMERA_SCCB_SDA  7\n",
                    encoding="utf-8",
                )
            errors = POLICY.check_camera_sccb_pin_contract(
                kconfig, defaults, board_header, camera_sketches
            )

        self.assertTrue(any("Example 12 camera SCL fallback" in error for error in errors))
        self.assertTrue(any("Example 12 camera SDA fallback" in error for error in errors))

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

if __name__ == "__main__":
    unittest.main()
