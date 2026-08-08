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

Both workflows run on every pull request and on pushes to `main`. Path filtering
is performed inside the repository by a versioned classifier, rather than only
by GitHub's workflow trigger filters, so a required check is still reported for
documentation-only changes.

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

Renames route both the old and new paths. The old
`example/ESP-IDF` spelling is recognized only so migrations and stale diffs are
classified safely; the canonical project root is `examples/esp-idf`.

Manual dispatch accepts `all`, one example directory name such as
`04_wifistation`, or a repository-relative path inside an example. Absolute,
missing, or repository-escaping selectors are rejected.

The classifier has a synthetic acceptance suite covering documentation-only,
single-project, shared, workflow, firmware, unknown, rename, invalid-scope, and
manual-selector cases. Those tests run before the build matrix is created.

## Discovery and build matrix

Only direct child directories of `examples/esp-idf/` that contain a root
`CMakeLists.txt` are first-party projects. Nested component examples are not
matrix entries. Discovery currently yields 12 projects.

Every selected project targets `esp32p4` and is compiled against these exact
stable ESP-IDF tags:

- `v5.5.5`
- `v6.0.2`

The matrix uses `fail-fast: false` and a bounded parallelism of six. Component
Manager downloads and ccache data are isolated by runner OS, ESP-IDF version,
target, project, and dependency-manifest hash.

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

Every successful matrix entry packages only files referenced by that project's
`build/flasher_args.json`. Each artifact contains:

- `manifest.json` with project, target, ESP-IDF version, commit, offsets, sizes,
  and SHA-256 hashes;
- portable `flasher_args.json` and `flash_args` metadata;
- the exact bootloader, partition table, application, and other referenced
  binaries;
- `flash.sh` and `flash.bat` helpers.

Artifacts are retained for 14 days. Paths are validated to remain inside the
selected project's build directory before packaging.

## Immutable firmware boundary

[`firmware/ESP32-P4-WiFi6-LCD-3in5.bin`](../firmware/ESP32-P4-WiFi6-LCD-3in5.bin)
is a separately delivered factory image. Source CI never builds, copies, wraps,
or re-uploads it. A change to a delivered `.bin` or `.zip` is classified for
explicit release review and must include provenance, version, target hardware,
flash instructions, and hardware-validation evidence supplied by the
maintainer.

## Validation boundary

A successful Actions run proves only that the selected source projects compile
and package with the recorded framework versions. It does not prove runtime
compatibility of the ESP32-C6 coprocessor firmware, nor validate display, touch,
camera, audio, storage, USB, power, or radio behavior. Those remain board-level
acceptance tests. This repository intentionally uses post-commit Actions as its
compile evidence; the maintenance workflow does not perform a local ESP-IDF
build.
