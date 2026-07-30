# Continuous Integration

This repository keeps documentation validation and firmware compilation as
separate workflows so that each check has a clear purpose.

## Workflow responsibilities

- [`docs.yml`](../.github/workflows/docs.yml) validates the bilingual top-level
  README files, local links, the product image, and the documented example
  inventory.
- [`esp-idf.yml`](../.github/workflows/esp-idf.yml) discovers and builds the
  first-party ESP-IDF projects. README-only and governance-only changes do not
  trigger firmware builds.

The ESP-IDF workflow runs for build-impacting, non-Markdown changes below
`example/ESP-IDF/`, for changes to its discovery or packaging helpers, and for
changes to the workflow itself. Nested component test apps are excluded because
they are not first-party matrix projects. A push to `main` uses the same paths.

## Project discovery and version matrix

Only direct child directories of `example/ESP-IDF/` that contain a
`CMakeLists.txt` file are first-party projects. Nested projects inside bundled
components are intentionally excluded.

Each selected project is built for `esp32p4` against two explicitly pinned
stable ESP-IDF tags:

- the latest maintained patch in the ESP-IDF v5.5 line at the time the workflow
  is updated;
- the latest stable ESP-IDF v6 release.

The exact tags are recorded in `esp-idf.yml`. Update them only after checking
the official ESP-IDF releases and the migration guides required by the new
version. The build matrix uses `fail-fast: false` so one compatibility failure
does not hide results from the other projects.

Component Manager downloads and ccache data are restored with a key scoped by
runner OS, ESP-IDF version, target, project, and dependency-manifest hash. This
keeps the two framework lines and 12 projects isolated while allowing repeated
CI runs to reuse safe downloads and compiler outputs.

A manual run accepts `all`, an example directory name such as
`04_wifistation`, or a repository-relative path inside an example. Invalid,
absolute, or repository-escaping paths are rejected before the matrix starts.

## Build artifacts

Every successful matrix entry uploads a flashable artifact generated only from
that project's `build/flasher_args.json`. Each artifact contains:

- `manifest.json` with the project, target, ESP-IDF version, commit, offsets,
  sizes, and SHA-256 hashes;
- portable `flasher_args.json` and `flash_args` generated from ESP-IDF metadata;
- the exact binaries referenced by the flasher metadata;
- `flash.sh` and `flash.bat` helpers. Run `sh flash.sh` on POSIX systems or
  `flash.bat` on Windows after connecting the board.

Artifacts are retained for 14 days. The checked-in image under `firmware/` is a
prebuilt product image and is never collected or re-uploaded by source-build
CI.

## Validation boundary

A successful workflow proves that the selected source projects compile with
the recorded framework versions. It does not replace hardware testing or prove
that an ESP32-C6 coprocessor firmware image is runtime-compatible with every
host component version. Display, touch, audio, camera, storage, power, and
hosted-wireless behavior still require board-level validation.
