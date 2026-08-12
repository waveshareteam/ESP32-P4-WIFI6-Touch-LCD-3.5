from __future__ import annotations

import argparse
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_esp_idf", SCRIPT_DIR / "package_esp_idf.py")
assert SPEC and SPEC.loader
package = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = package
SPEC.loader.exec_module(package)


class PackageEspIdfTests(unittest.TestCase):
    @staticmethod
    def p4_image() -> bytes:
        header = bytearray(24)
        header[0] = package.ESP_IMAGE_MAGIC
        header[1] = 1
        header[12:14] = package.ESP32P4_IMAGE_CHIP_ID.to_bytes(2, "little")
        return bytes(header)

    def make_project(self, root: Path, profile: str = "rev1_3") -> argparse.Namespace:
        project = root / "example"
        build = project / f"build-{profile}"
        (build / "config").mkdir(parents=True)
        (project / "CMakeLists.txt").write_text("project(test)\n", encoding="utf-8")
        for name, values in package.PROFILE_VALUES.items():
            (project / f"sdkconfig.defaults.{name}").write_text("\n".join(f"{key}={value}" for key, value in values.items()) + "\n", encoding="utf-8")
        (build / "config" / "sdkconfig.json").write_text(json.dumps(package.PROFILE_VALUES[profile]), encoding="utf-8")
        (build / "boot.bin").write_bytes(b"boot")
        (build / "app.bin").write_bytes(self.p4_image())
        (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x1000": "boot.bin", "0x10000": "app.bin"}, "write_flash_args": ["--flash_mode", "dio"]}), encoding="utf-8")
        return argparse.Namespace(project="example", idf_version="v6.0.2", profile=profile, build_dir=f"build-{profile}", target="esp32p4", git_sha="0123456789ABCDEF0123456789ABCDEF01234567", baud=460800, output_root="artifacts")

    def test_package_includes_profile_schema_and_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = package.REPOSITORY_ROOT
            package.REPOSITORY_ROOT = root
            try:
                args = self.make_project(root)
                artifact = package.package(args)
                manifest = json.loads((artifact / "manifest.json").read_text(encoding="utf-8"))
                self.assertEqual((manifest["schema_version"], manifest["profile"], manifest["artifact_kind"]), (2, "rev1_3", "esp-idf-flashable"))
                self.assertTrue(manifest["host_only"])
                self.assertFalse(manifest["contains_c6_firmware"])
                self.assertTrue(all(entry["sha256"] and entry["size"] > 0 for entry in manifest["files"]))
                self.assertEqual(manifest["git_sha"], args.git_sha.lower())
                self.assertEqual(manifest["files"][1]["image_chip_id"], package.ESP32P4_IMAGE_CHIP_ID)
                self.assertNotIn("erase", manifest["flash_command"])
                self.assertEqual(manifest["artifact_policy_capacity_bytes"], 32 * 1024 * 1024)
                self.assertEqual(manifest["device_flash_capacity_bytes"], 16 * 1024 * 1024)
                self.assertIsNone(manifest["declared_flash_size_bytes"])
            finally:
                package.REPOSITORY_ROOT = original

    def test_profile_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = package.REPOSITORY_ROOT
            package.REPOSITORY_ROOT = root
            try:
                args = self.make_project(root)
                config = root / "example" / "build-rev1_3" / "config" / "sdkconfig.json"
                config.write_text(json.dumps({"CONFIG_ESP32P4_SELECTS_REV_LESS_V3": False}), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "does not match"):
                    package.package(args)
            finally:
                package.REPOSITORY_ROOT = original

    def test_profile_validation_applies_kconfig_disabled_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = package.REPOSITORY_ROOT
            package.REPOSITORY_ROOT = root
            try:
                args = self.make_project(root)
                config = root / "example" / "build-rev1_3" / "config" / "sdkconfig.json"
                config.write_text(
                    json.dumps(
                        {
                            "CONFIG_ESP32P4_SELECTS_REV_LESS_V3": True,
                            "CONFIG_ESP32P4_REV_MIN_100": "y",
                        }
                    ),
                    encoding="utf-8",
                )
                package.validate_profile(root / "example", config.parents[1], "rev1_3")

                config.write_text(json.dumps({"CONFIG_ESP32P4_REV_MIN_100": "y"}), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "SELECTS_REV_LESS_V3=y"):
                    package.validate_profile(root / "example", config.parents[1], "rev1_3")

                args = self.make_project(root, "rev3_x")
                config = root / "example" / "build-rev3_x" / "config" / "sdkconfig.json"
                config.write_text(json.dumps({"CONFIG_ESP32P4_REV_MIN_300": True}), encoding="utf-8")
                package.validate_profile(root / "example", config.parents[1], "rev3_x")
            finally:
                package.REPOSITORY_ROOT = original

    def test_overlap_capacity_c6_and_erase_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = package.REPOSITORY_ROOT
            package.REPOSITORY_ROOT = root
            try:
                args = self.make_project(root)
                build = root / "example" / "build-rev1_3"
                for flash_files, expected in (({"0x1000": "boot.bin", "0x1002": "app.bin"}, "overlaps"), ({hex(package.DEVICE_FLASH_CAPACITY): "app.bin"}, "16 MiB")):
                    (build / "flasher_args.json").write_text(json.dumps({"flash_files": flash_files}), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, expected):
                        package.package(args)
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x1000": "boot.bin"}, "write_flash_args": ["--erase-all"]}), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "erase"):
                    package.package(args)
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x1000": "boot.bin"}, "helper": "erase-region"}), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "erase"):
                    package.package(args)
                args.git_sha = "not-a-full-sha"
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x1000": "boot.bin", "0x10000": "app.bin"}}), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "40-hex"):
                    package.package(args)
                args.git_sha = "0123456789abcdef0123456789abcdef01234567"
                c6 = bytearray(self.p4_image())
                c6[12:14] = (13).to_bytes(2, "little")
                (build / "app.bin").write_bytes(bytes(c6))
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x10000": "app.bin"}}), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "not ESP32-P4"):
                    package.package(args)
                (build / "app.bin").write_bytes(b"\xe9\x01")
                with self.assertRaisesRegex(ValueError, "truncated"):
                    package.package(args)
                args.target = "esp32c6"
                with self.assertRaisesRegex(ValueError, "esp32p4"):
                    package.package(args)
                args.target = "esp32p4"
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x1000": "c6.bin"}}), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "C6"):
                    package.package(args)
            finally:
                package.REPOSITORY_ROOT = original

    def test_declared_flash_size_contract(self) -> None:
        self.assertEqual(
            package.declared_flash_size_bytes({"write_flash_args": ["--flash-size", "2MB"]}),
            2 * 1024 * 1024,
        )
        self.assertEqual(
            package.declared_flash_size_bytes({"write_flash_args": ["--flash_size", "16mb"]}),
            16 * 1024 * 1024,
        )
        self.assertEqual(
            package.declared_flash_size_bytes({"write_flash_args": ["--flash-size=4MB"]}),
            4 * 1024 * 1024,
        )
        self.assertEqual(
            package.declared_flash_size_bytes({"write_flash_args": ["--flash_size=8mb"]}),
            8 * 1024 * 1024,
        )
        self.assertIsNone(
            package.declared_flash_size_bytes(
                {"write_flash_args": ["--flash-size-extra", "32MB"]}
            )
        )
        for arguments, expected in (
            (["--flash-size"], "missing"),
            (["--flash-size="], "missing"),
            (["--flash_size="], "missing"),
            (["--flash-size", "1MB"], "unsupported"),
            (["--flash-size", "32MB"], "unsupported"),
            (["--flash-size=32MB"], "unsupported"),
            (["--flash-size", "2MB", "--flash_size", "2MB"], "duplicate"),
            (["--flash-size", "2MB", "--flash_size", "4MB"], "duplicate"),
            (["--flash-size=2MB", "--flash_size", "2MB"], "duplicate"),
            (["--flash-size=2MB", "--flash_size=4MB"], "duplicate"),
        ):
            with self.subTest(arguments=arguments), self.assertRaisesRegex(ValueError, expected):
                package.declared_flash_size_bytes({"write_flash_args": arguments})

    def test_declared_capacity_bounds(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = package.REPOSITORY_ROOT
            package.REPOSITORY_ROOT = root
            try:
                args = self.make_project(root)
                build = root / "example" / "build-rev1_3"
                (build / "flasher_args.json").write_text(
                    json.dumps(
                        {
                            "flash_files": {"0x1000": "boot.bin", "0x10000": "app.bin"},
                            "write_flash_args": ["--flash-size", "2MB"],
                        }
                    ),
                    encoding="utf-8",
                )
                artifact = package.package(args)
                manifest = json.loads((artifact / "manifest.json").read_text(encoding="utf-8"))
                self.assertEqual(manifest["declared_flash_size_bytes"], 2 * 1024 * 1024)

                args.output_root = "artifacts-over-declared"
                (build / "flasher_args.json").write_text(
                    json.dumps(
                        {
                            "flash_files": {"0x200000": "app.bin"},
                            "write_flash_args": ["--flash_size", "2MB"],
                        }
                    ),
                    encoding="utf-8",
                )
                with self.assertRaisesRegex(ValueError, "2 MiB"):
                    package.package(args)

                args.output_root = "artifacts-16mb-boundary"
                boundary = package.DEVICE_FLASH_CAPACITY - len(self.p4_image())
                (build / "flasher_args.json").write_text(
                    json.dumps(
                        {
                            "flash_files": {hex(boundary): "app.bin"},
                            "write_flash_args": ["--flash-size", "16MB"],
                        }
                    ),
                    encoding="utf-8",
                )
                package.package(args)

                args.output_root = "artifacts-past-16mb"
                (build / "flasher_args.json").write_text(
                    json.dumps(
                        {
                            "flash_files": {hex(boundary + 1): "app.bin"},
                            "write_flash_args": ["--flash-size", "16MB"],
                        }
                    ),
                    encoding="utf-8",
                )
                with self.assertRaisesRegex(ValueError, "16 MiB"):
                    package.package(args)
            finally:
                package.REPOSITORY_ROOT = original


if __name__ == "__main__":
    unittest.main()
