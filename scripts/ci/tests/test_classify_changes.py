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

        pull_request_template = self.report(
            "M\t.github/pull_request_template.md"
        )
        self.assertEqual(pull_request_template["esp_idf"]["mode"], "none")
        self.assertTrue(pull_request_template["scope"]["docs_only"])

        gitignore = self.report("M\t.gitignore")
        self.assertEqual(gitignore["esp_idf"]["mode"], "none")
        self.assertTrue(gitignore["scope"]["docs_only"])

    def test_documentation_policy_configs_select_no_builds(self) -> None:
        for changed_path in (
            "config/markdown-audit.json",
            "config/ci-routing.json",
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
            "scripts/ci/package_esp_idf.py",
        ):
            with self.subTest(changed_path=changed_path):
                report = self.report(f"M\t{changed_path}")
                self.assertEqual(report["esp_idf"]["mode"], "all")
                self.assertEqual(len(self.selected_names(report)), 12)

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
            self.assertEqual(
                json.loads(outputs["examples"])[0]["name"],
                "09_video_lcd_display",
            )


if __name__ == "__main__":
    unittest.main()
