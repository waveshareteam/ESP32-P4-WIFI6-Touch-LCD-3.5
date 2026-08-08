# Contributing

[简体中文](CONTRIBUTING_ZH.md)

Thank you for improving the ESP32-P4-WIFI6-Touch-LCD-3.5 repository. Changes
should stay product-specific, reproducible, and explicit about what was proven
by CI versus physical hardware.

## Before changing code

1. Check the current branch, upstream, and working tree. Preserve unrelated
   user changes.
2. Use the canonical project root [`examples/esp-idf/`](examples/esp-idf/).
3. Read the [CI policy](docs/ci.md) and, for component work, the
   [component policy](docs/components.md).
4. Confirm hardware assignments against both the product schematic and the
   relevant BSP header. Do not copy pin maps from a similarly named board.

## Change rules

- Keep English and Simplified-Chinese first-party documentation synchronized.
- Do not add Arduino instructions unless a maintained Arduino project is added
  to this repository.
- Keep example-local BSP variants separate until their API, dependency,
  Kconfig, color-order, and hardware behavior are proven equivalent.
- Use bounded managed-component versions. Explain exact pins or IDF-conditional
  versions next to the manifest entry.
- Do not edit or regenerate the checked-in factory image as part of a source
  change. Firmware delivery requires a separate, maintainer-approved release
  record and hardware evidence.
- Do not commit `build/`, `managed_components/`, caches, credentials, Wi-Fi
  passwords, private filesystem paths, customer data, or serial logs containing
  secrets.

## Static checks

These repository checks do not compile firmware and may be run before pushing:

```text
python3 scripts/check_readme.py
python3 -m unittest discover -s scripts/ci/tests -p "test_*.py" -v
python3 scripts/ci/classify_changes.py --working-tree
```

The authoritative compile result is the post-commit GitHub Actions matrix for
ESP-IDF v5.5.5 and v6.0.2. Do not describe a change as hardware-validated unless
the target board and tested peripherals are identified in the pull request.

## Pull requests

Keep a pull request focused and include:

- the problem and intended behavior;
- affected example paths and hardware revision, if known;
- dependency or configuration changes;
- static-check output;
- links to the final-sha Actions results;
- separate board-test evidence for display, touch, camera, audio, storage, USB,
  power, or ESP32-C6 hosted Wi-Fi changes;
- confirmation that the checked-in factory image was not changed, or complete
  release provenance if firmware delivery is the explicit scope.
