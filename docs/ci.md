# Continuous Integration

[简体中文](ci_ZH.md)

The repository separates documentation validation, change classification, and
firmware compilation so every required check has a stable meaning.

## Workflow responsibilities

- [`docs.yml`](../.github/workflows/docs.yml) validates the first-party
  bilingual documentation, local links, product image, and example inventory.
- [`esp-idf.yml`](../.github/workflows/esp-idf.yml) classifies the complete Git
  change set, selects affected first-party projects, builds the selected matrix,
  packages flashable artifacts, and reports one aggregate result.
- [`product-firmware.yml`](../.github/workflows/product-firmware.yml) builds the
  maintained product source once for each revision profile when conservative
  routing says that product firmware is affected.
- [`arduino-policy.yml`](../.github/workflows/arduino-policy.yml) discovers and,
  when Arduino or shared build inputs change, compiles the ten first-party
  Arduino sketches with Arduino-ESP32 3.3.11 and the default
  `ChipVariant=postv3` configuration.
- [`repository-policy.yml`](../.github/workflows/repository-policy.yml) runs the
  deterministic profile, packaging, routing, and Windows flasher contracts.

These workflows run on every pull request and on pushes to `main`. Path filtering
is performed inside the repository by a versioned classifier, rather than only
by GitHub's workflow trigger filters, so a required check is still reported for
documentation-only changes.
For a change that does not affect Arduino, the stable Arduino aggregate check
still runs while its compile matrix is skipped.

## Change routing

[`classify_changes.py`](../scripts/ci/classify_changes.py) obtains a complete
merge-base diff for pull requests and pushes. It refuses an empty, malformed, or
unsafe scope rather than silently assuming that every project should build.

| Change class | ESP-IDF route |
| --- | --- |
| Root, docs, governance, schematic, assets, or example Markdown | No example build |
| Source/configuration inside one direct example | That example only |
| Shared example source, root build input, workflow, or CI helper | All discovered examples |
| Unknown non-document path | All examples and report the unknown path |
| Bundled Brookesia `test_apps` | No product-example build |
| Checked-in firmware/release delivery | No source build; flag delivery review |

Arduino sketch or bundled board-library source is routed to the independent
Arduino matrix and does not select an ESP-IDF example by itself. Shared CI,
workflow, or profile inputs conservatively select both applicable matrices.

Renames route both the old and new paths so migrations and stale diffs are
classified safely. The canonical ESP-IDF project root is `examples/esp-idf`.

Manual dispatch accepts `all`, one example directory name such as
`04_wifistation`, or a repository-relative path inside an example. Absolute,
missing, or repository-escaping selectors are rejected.

The classifier has a synthetic acceptance suite covering documentation-only,
single-project, shared, workflow, firmware, unknown, rename, invalid-scope, and
manual-selector cases. Those tests run before the build matrix is created.

## Discovery and build matrix

Only direct child directories of `examples/esp-idf/` that contain a root
`CMakeLists.txt` are first-party projects. Nested component examples are not
matrix entries. Discovery currently yields 12 projects for this single product.

Every selected project targets `esp32p4` and is compiled against these exact
stable ESP-IDF tags:

- `v5.5.5`
- `v6.0.2`

The standard example matrix is 12 projects × 2 ESP-IDF versions = 24 builds;
all of those builds use the `rev3_x` profile. It is not doubled for every
silicon-revision profile. When its route is selected, Arduino CI separately discovers the ten sketches in
[`examples/arduino/`](../examples/arduino/), compiles them with
Arduino-ESP32 3.3.11, and defaults to `ChipVariant=postv3`. `ChipVariant=prev3`
is solely for confirmed rev1.x silicon, including rev1.3. The Arduino board
library requires GFX Library for Arduino 1.6.7; the LVGL sketch requires LVGL
9.3.0. See the [Arduino guide](../examples/arduino/README.md).

The matrix uses `fail-fast: false` and a bounded parallelism of six. Component
Manager downloads and ccache data are isolated by runner OS, ESP-IDF version,
target, project, and dependency-manifest hash.

## ESP32-P4 revision profiles

The default ESP-IDF profile for fresh example configurations is `rev3_x`
(rev3.x silicon, `[3.0, 4.0)`):

- `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n`
- `CONFIG_ESP32P4_REV_MIN_300=y`

The explicit `rev1_3` compatibility profile is for confirmed rev1.x silicon
(`[1.0, 2.0)`, including rev1.3):

- `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`
- `CONFIG_ESP32P4_REV_MIN_100=y`

To select `rev1_3` locally, configure a separate build directory from the
example project directory:

```text
idf.py -B build-rev1_3 -D SDKCONFIG=sdkconfig.rev1_3 -D WAVESHARE_REVISION_PROFILE=rev1_3 set-target esp32p4
idf.py -B build-rev1_3 -D SDKCONFIG=sdkconfig.rev1_3 -D WAVESHARE_REVISION_PROFILE=rev1_3 build
```

Profiles have independent SDK configuration files and build directories. Their
names identify ESP32-P4 silicon rather than PCB or product hardware revision;
binaries are incompatible and must not be substituted for one another. Both
supported IDF lines generate `CONFIG_ESP32P4_REV_MAX_FULL=199` for `rev1_3`,
so the historical `SELECTS_REV_LESS_V3` name does not make 2.x silicon
compatible. Revisions outside `[1.0, 2.0)` and `[3.0, 4.0)` are unsupported and
the flasher rejects them. The maintained product source is
[`12_esp32-p4-eye`](../examples/esp-idf/12_esp32-p4-eye/): on ESP-IDF v6.0.2 it
produces separate `rev1_3` and `rev3_x` product jobs and artifacts. This is a
product-specific compatibility surface, not a second matrix for every example.
The [revision guide](revisions.md) records the matching MIPI DSI clock rule and
the distinction between this board's SPI display and its camera CSI path.

Update a framework tag only after checking the official ESP-IDF release and all
migration guides between the old and new versions. For the current matrix, the
major-version transition is covered by the official
[ESP-IDF 5.5 to 6.0 migration guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/migration-guides/release-6.x/6.0/index.html).

## Aggregate result

`ESP-IDF build matrix` is the stable final gate:

- classification or routing-test failure fails the gate;
- zero selected projects succeeds only when the build job is skipped;
- one or more selected projects succeeds only when the entire generated matrix
  succeeds.

This job is the suitable branch-protection check. Individual matrix job names
change as projects and framework versions change.

## Build artifacts

Every successful matrix entry packages only files referenced by that profile's
`flasher_args.json`; checkout, artifact names, and manifests are bound to the PR
branch's final HEAD rather than GitHub's temporary merge commit. The product
artifacts target the ESP32-P4 host only: they do not package ESP32-C6 firmware,
forbid explicit full-chip or region erase operations, and keep three independent
capacity contracts: a 32 MiB artifact-policy ceiling, the product's 16 MiB
physical external NOR capacity, and (when `flasher_args.json` declares
`--flash-size` or `--flash_size`) the supported 2/4/8/16 MB esptool declaration.
The effective flash-plan limit is the lowest applicable value; absent declarations
still use the 16 MiB physical cap. The schema-2 manifest records all three values
so the Windows flasher independently enforces the same limit. Normal `write_flash`
operation may erase only the sectors it writes. The
packager and flasher independently validate every ESP image header as ESP32-P4
chip ID 18, while still allowing raw partition, NVS, and data entries. Each
package is also checked for expected offsets, SHA-256 hashes, and file sizes.
Each artifact contains:

- `manifest.json` with project, target, ESP-IDF version, commit, offsets, sizes,
  SHA-256 hashes, policy/device capacities, and the nullable declared flash size;
- portable `flasher_args.json` and `flash_args` metadata;
- the exact bootloader, partition table, application, and other referenced
  binaries;
- `flash.sh` and `flash.bat` helpers.

Artifacts are retained for 14 days. Paths are validated to remain inside the
selected profile's build directory before packaging.

The generated flasher probes, then re-probes, the ESP32-P4 silicon revision
before flashing: `[1.0, 2.0)` accepts only `rev1_3`, while `[3.0, 4.0)` accepts
only `rev3_x`; every other revision is rejected. Silicon revision identifies neither the PCB
revision nor the board's electrical revision.

## Windows CI firmware test flow

From a clean checkout of the exact, non-Draft pull-request head, use the root
CMD entry point:

```text
Flash-CI-Firmware.cmd -SelfTest
Flash-CI-Firmware.cmd -ListOnly
Flash-CI-Firmware.cmd -Port COMx
```

The interactive command requires Git, an authenticated GitHub CLI, and a Python
environment containing `esptool`. It accepts only successful Actions runs and
profile-qualified artifacts whose SHA exactly matches the local branch and the
open Ready-for-review pull request. `-ListOnly` reports the complete 26-item
contract: 24 default-profile example artifacts plus the two maintained-product
profiles. At runtime, rev3.x silicon selects the 24 examples and the
`rev3_x` product artifact (25 items); rev1.x silicon selects only the explicit
`rev1_3` product artifact.

The tool downloads and validates one artifact, re-probes the chip, writes only
the manifest flash plan, and then stops. It never advances automatically: test
the board and click **Mark PASS and flash next** only after the current firmware
has passed the required hardware checks. Progress is bound to the final SHA,
artifact build SHA, profile, and normalized COM port; changing ports resets the
saved confirmations. Attempts, PASS results, downloaded packages, and log paths
are retained under the user's local application-data directory. These records
document the manual sequence but do not by themselves constitute HIL evidence.

## Immutable firmware boundary

[`firmware/ESP32-P4-WiFi6-LCD-3in5.bin`](../firmware/ESP32-P4-WiFi6-LCD-3in5.bin)
is a separately delivered factory image. Source CI never builds, copies, wraps,
or re-uploads it. A change to a delivered `.bin` or `.zip` is classified for
explicit release review and must include provenance, version, target hardware,
flash instructions, and hardware-validation evidence supplied by the
maintainer.

## Validation boundary

A successful Actions run proves only that the selected source projects compile
and package with the recorded framework versions. It is not hardware-in-the-loop
(HIL) evidence and does not prove runtime compatibility of the ESP32-C6
coprocessor firmware, or validate display, touch, camera, audio, storage, USB,
power, radio, or Arduino sketch behavior. Those remain board-level acceptance tests. This
repository intentionally uses post-commit Actions as its compile evidence; the
maintenance workflow does not perform a local ESP-IDF build.
