from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

SPEC = importlib.util.spec_from_file_location(
    "classify_changes", SCRIPT_DIR / "classify_changes.py"
)
assert SPEC and SPEC.loader
classify = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = classify
SPEC.loader.exec_module(classify)


class ClassifyChangesTests(unittest.TestCase):
    def report(self, *records: str) -> dict[str, object]:
        return classify.classify_changes(classify.parse_name_status(list(records)))

    def selected_names(self, report: dict[str, object]) -> list[str]:
        return [entry["name"] for entry in report["esp_idf"]["selected"]]

    def test_root_and_example_markdown_select_no_builds(self) -> None:
        report = self.report(
            "M\tREADME.md",
            "M\texamples/esp-idf/09_video_lcd_display/README.md",
        )
        self.assertEqual(report["esp_idf"]["mode"], "none")
        self.assertTrue(report["scope"]["docs_only"])
        self.assertFalse(report["scope"]["product_firmware_required"])

        pull_request_template = self.report(
            "M\t.github/pull_request_template.md"
        )
        self.assertEqual(pull_request_template["esp_idf"]["mode"], "none")
        self.assertTrue(pull_request_template["scope"]["docs_only"])

        for ignored_root_file in (".gitattributes", ".gitignore"):
            with self.subTest(ignored_root_file=ignored_root_file):
                ignored = self.report(f"M\t{ignored_root_file}")
                self.assertEqual(ignored["esp_idf"]["mode"], "none")
                self.assertTrue(ignored["scope"]["docs_only"])
                self.assertEqual(ignored["unknown_paths"], [])

    def test_documentation_policy_configs_select_no_builds(self) -> None:
        for changed_path in (
            "config/markdown-audit.json",
            "config/ci-routing.json",
            "scripts/check_readme.py",
        ):
            with self.subTest(changed_path=changed_path):
                report = self.report(f"M\t{changed_path}")
                self.assertEqual(report["esp_idf"]["mode"], "none")
                self.assertEqual(self.selected_names(report), [])
                self.assertTrue(report["scope"]["docs_only"])

    def test_direct_source_selects_only_affected_project(self) -> None:
        report = self.report(
            "M\texamples/esp-idf/09_video_lcd_display/main/app_video.c"
        )
        self.assertEqual(report["esp_idf"]["mode"], "selected")
        self.assertEqual(self.selected_names(report), ["09_video_lcd_display"])
        self.assertFalse(report["scope"]["product_firmware_required"])

    def test_arduino_surface_does_not_select_esp_idf_projects(self) -> None:
        source = self.report(
            "M\texamples/arduino/examples/01_HelloWorld/01_HelloWorld.ino"
        )
        self.assertEqual(source["esp_idf"]["mode"], "none")
        self.assertEqual(self.selected_names(source), [])
        self.assertFalse(source["scope"]["docs_only"])
        self.assertFalse(source["scope"]["product_firmware_required"])
        self.assertTrue(source["scope"]["arduino_build_required"])
        self.assertEqual(source["unknown_paths"], [])
        self.assertEqual(source["routes"][0]["kind"], "arduino_source")

        documentation = self.report(
            "M\texamples/arduino/examples/01_HelloWorld/README.md"
        )
        self.assertEqual(documentation["esp_idf"]["mode"], "none")
        self.assertTrue(documentation["scope"]["docs_only"])
        self.assertFalse(documentation["scope"]["product_firmware_required"])
        self.assertFalse(documentation["scope"]["arduino_build_required"])
        self.assertEqual(documentation["unknown_paths"], [])
        self.assertEqual(documentation["routes"][0]["kind"], "arduino_documentation")

    def test_product_firmware_routing_is_conservative(self) -> None:
        product = self.report(
            "M\texamples/esp-idf/12_esp32-p4-eye/main/main.c"
        )
        self.assertTrue(product["scope"]["product_firmware_required"])
        profile = self.report("M\tconfig/revision-profiles.json")
        self.assertTrue(profile["scope"]["product_firmware_required"])
        unknown = self.report("A\ttools/new_generator.py")
        self.assertTrue(unknown["scope"]["product_firmware_required"])
        self.assertTrue(unknown["scope"]["arduino_build_required"])

    def test_esp_idf_source_does_not_select_arduino_builds(self) -> None:
        report = self.report(
            "M\texamples/esp-idf/09_video_lcd_display/main/app_video.c"
        )
        self.assertFalse(report["scope"]["arduino_build_required"])

        revision_policy = self.report("M\tconfig/revision-profiles.json")
        self.assertTrue(revision_policy["scope"]["arduino_build_required"])

        discovery = self.report("M\tscripts/discover_arduino_examples.py")
        self.assertEqual(discovery["esp_idf"]["mode"], "none")
        self.assertTrue(discovery["scope"]["arduino_build_required"])
        self.assertEqual(discovery["unknown_paths"], [])

    def test_cmake_is_build_input_but_readme_txt_is_documentation(self) -> None:
        cmake = self.report(
            "M\texamples/esp-idf/09_video_lcd_display/CMakeLists.txt"
        )
        readme = self.report(
            "M\texamples/esp-idf/04_wifistation/README.txt"
        )
        self.assertEqual(self.selected_names(cmake), ["09_video_lcd_display"])
        self.assertEqual(readme["esp_idf"]["mode"], "none")
        self.assertTrue(readme["scope"]["docs_only"])

    def test_shared_source_and_workflow_select_all_projects(self) -> None:
        for changed_path in (
            "examples/esp-idf/common/compat.c",
            ".github/workflows/docs.yml",
            "Flash-CI-Firmware.cmd",
            "scripts/Flash-CI-Firmware.ps1",
            "scripts/ci/package_esp_idf.py",
        ):
            with self.subTest(changed_path=changed_path):
                report = self.report(f"M\t{changed_path}")
                self.assertEqual(report["esp_idf"]["mode"], "all")
                self.assertEqual(len(self.selected_names(report)), 12)
                self.assertTrue(report["scope"]["product_firmware_required"])

    def test_mixed_arduino_and_global_change_remains_global(self) -> None:
        report = self.report(
            "M\texamples/arduino/examples/01_HelloWorld/01_HelloWorld.ino",
            "M\t.github/workflows/esp-idf.yml",
        )
        self.assertEqual(report["esp_idf"]["mode"], "all")
        self.assertEqual(len(self.selected_names(report)), 12)
        self.assertTrue(report["scope"]["product_firmware_required"])

    def test_firmware_files_never_enter_example_matrix(self) -> None:
        markdown = self.report("M\tfirmware/README.md")
        self.assertEqual(markdown["esp_idf"]["mode"], "none")
        self.assertTrue(markdown["scope"]["docs_only"])
        self.assertTrue(markdown["scope"]["firmware_touched"])

        binary = self.report("M\tfirmware/factory.bin")
        self.assertEqual(binary["esp_idf"]["mode"], "none")
        self.assertFalse(binary["scope"]["docs_only"])
        self.assertTrue(binary["scope"]["release_review_required"])

    def test_embedded_brookesia_test_app_is_not_a_product_example(self) -> None:
        report = self.report(
            "M\texamples/esp-idf/11_esp_brookesia_phone/"
            "components/brookesia_core/test_apps/main/test.c"
        )
        self.assertEqual(report["esp_idf"]["mode"], "none")
        self.assertFalse(report["scope"]["docs_only"])

    def test_rename_uses_old_and_new_paths(self) -> None:
        report = self.report(
            "R100\texample/ESP-IDF/09_video_lcd_display/main/app_video.c\t"
            "examples/esp-idf/09_video_lcd_display/main/app_video.c"
        )
        self.assertEqual(self.selected_names(report), ["09_video_lcd_display"])

    def test_unknown_path_is_conservative_and_visible(self) -> None:
        report = self.report("A\ttools/new_generator.py")
        self.assertEqual(report["esp_idf"]["mode"], "all")
        self.assertEqual(report["unknown_paths"], ["tools/new_generator.py"])

    def test_empty_or_malformed_scope_fails(self) -> None:
        with self.assertRaises(classify.ClassificationError):
            classify.parse_name_status([])
        with self.assertRaises(classify.ClassificationError):
            classify.parse_name_status(["R100\tonly-one-path"])
        with self.assertRaises(classify.ClassificationError):
            classify.parse_name_status(["M\t../outside.c"])

    def test_manual_selector_supports_name_and_path(self) -> None:
        by_name = classify.classify_selector("09_video_lcd_display")
        by_path = classify.classify_selector(
            "examples/esp-idf/09_video_lcd_display/main/app_video.c"
        )
        self.assertEqual(self.selected_names(by_name), ["09_video_lcd_display"])
        self.assertEqual(self.selected_names(by_path), ["09_video_lcd_display"])
        product = classify.classify_selector("12_esp32-p4-eye")
        self.assertTrue(product["scope"]["product_firmware_required"])

    def test_exact_workflow_cli_writes_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            changed = temporary / "changed.txt"
            output = temporary / "github-output.txt"
            changed.write_text(
                "M\texamples/esp-idf/09_video_lcd_display/main/app_video.c\n",
                encoding="utf-8",
            )
            completed = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT_DIR / "classify_changes.py"),
                    "--changed-files-from",
                    str(changed),
                    "--github-output",
                    str(output),
                ],
                cwd=REPOSITORY_ROOT,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            outputs = dict(
                line.split("=", 1)
                for line in output.read_text(encoding="utf-8").splitlines()
            )
            self.assertEqual(outputs["mode"], "selected")
            self.assertEqual(outputs["count"], "1")
            self.assertEqual(outputs["product_firmware_required"], "false")
            self.assertEqual(outputs["arduino_build_required"], "false")
            self.assertEqual(
                json.loads(outputs["examples"])[0]["name"],
                "09_video_lcd_display",
            )


if __name__ == "__main__":
    unittest.main()
