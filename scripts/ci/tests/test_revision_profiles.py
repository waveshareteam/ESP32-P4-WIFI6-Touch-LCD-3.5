from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
ROOT = SCRIPT_DIR.parents[1]


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


if __name__ == "__main__":
    unittest.main()
