from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parents[1]
ROOT = SCRIPT_DIR.parents[1]
DISCOVERY_PATH = ROOT / "scripts" / "discover_arduino_examples.py"
SPEC = importlib.util.spec_from_file_location("discover_arduino_examples", DISCOVERY_PATH)
assert SPEC and SPEC.loader
DISCOVERY = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = DISCOVERY
SPEC.loader.exec_module(DISCOVERY)

FQBN = (
    "esp32:esp32:esp32p4:ChipVariant=postv3,PSRAM=enabled,FlashSize=16M,"
    "FlashMode=qio,FlashFreq=80,PartitionScheme=app3M_fat9M_16MB,"
    "UploadMode=default,UploadSpeed=921600"
)


class DiscoverArduinoExamplesTests(unittest.TestCase):
    def test_discovers_the_ten_first_party_sketches(self) -> None:
        sketches = DISCOVERY.discover()
        self.assertEqual(len(sketches), 10)
        self.assertEqual(sketches[0]["name"], "01_HelloWorld")
        self.assertEqual(sketches[-1]["name"], "10_Mic_Record")
        self.assertTrue(all(entry["path"].startswith("examples/arduino/examples/") for entry in sketches))

    def test_selector_accepts_name_and_repository_path(self) -> None:
        entry = DISCOVERY.discover()[2]
        self.assertTrue(DISCOVERY.selector_matches(entry["name"], entry["path"], entry["name"]))
        self.assertTrue(DISCOVERY.selector_matches(entry["name"], entry["path"], entry["path"]))
        self.assertFalse(DISCOVERY.selector_matches(entry["name"], entry["path"], "10_Mic_Record"))

    def test_workflow_invocation_writes_the_complete_matrix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "github-output.txt"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(DISCOVERY_PATH),
                    "--core",
                    "3.3.11",
                    "--fqbn",
                    FQBN,
                    "--github-output",
                    str(output),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            values = dict(line.split("=", 1) for line in output.read_text(encoding="utf-8").splitlines())
            matrix = json.loads(values["matrix"])
            self.assertEqual(values["count"], "10")
            self.assertEqual(len(matrix["include"]), 10)
            self.assertTrue(all(entry["core"] == "3.3.11" for entry in matrix["include"]))
            self.assertTrue(all(entry["fqbn"] == FQBN for entry in matrix["include"]))

    def test_workflow_invocation_rejects_unknown_selector(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "github-output.txt"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(DISCOVERY_PATH),
                    "--selector",
                    "does-not-exist",
                    "--core",
                    "3.3.11",
                    "--fqbn",
                    FQBN,
                    "--github-output",
                    str(output),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("matched no Arduino sketch", completed.stderr)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
