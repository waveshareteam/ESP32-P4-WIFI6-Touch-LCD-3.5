# Component Maintenance Policy

[简体中文](components_ZH.md)

This repository uses both managed components and project-local components. A
local component is not automatically technical debt: several directories carry
board behavior or example-specific integration that cannot be replaced without
changing runtime behavior.

## Current classification

| Component | Location | Decision |
| --- | --- | --- |
| ESP32-P4-WIFI6-Touch-LCD-3.5 BSP variants | Examples 07–12 | Keep project-local |
| `bsp_extra` | Examples 08, 09, and 12 | Keep as example-specific board glue |
| `sd_card` | Example 05 | Keep as example test support |
| `esp_painter` | Example 12 | Keep as product UI/rendering support |
| `esp_extractor` | Examples 10 and 11 | Keep embedded with its target-specific prebuilt library |
| Detection model wrappers | Example 12 | Keep locally; consume `espressif/esp-dl ==3.1.3` through Component Manager |
| `espressif/button` | Example 12 | Managed and pinned to `==4.2.0` |
| `espressif/esp_lvgl_port` | Example 12 | Managed and pinned to `==2.8.0~1` for the ESP-IDF 5.5/6.0 CI matrix |

The ESP-IDF Component Registry publishes
[`waveshare/esp32_p4_wifi6_touch_lcd_3_5` v2.0.0](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_3_5/versions/2.0.0).
It is a useful upstream reference, but it is not a drop-in replacement for the
six variants in this repository.

## Why the BSP variants remain local

The local and Registry BSPs expose broadly similar board APIs and the same
product GPIO assignments, but the audited variants are not behaviorally
equivalent:

- the local source/component names differ from the Registry package;
- the display color order differs in at least one path (local RGB versus
  upstream BGR), which can visibly swap red and blue;
- local Kconfig symbols and defaults are not identical;
- examples 07–11 use the `esp_lvgl_adapter`/LVGL `>=8,<10` integration profile,
  while example 12 uses `esp_lvgl_port` with LVGL `8.3.*`;
- each project currently carries the exact component graph used by its
  application.

Consolidation is therefore deferred until a migration proves API, Kconfig,
color-order, display timing, touch, audio, SD, USB, and LVGL equivalence on the
board. Both pinned ESP-IDF matrix lines must pass after that migration, followed
by hardware tests.

## Hardware-backed invariants

The schematic and local BSP headers agree on the product-critical GPIOs:

| Function | GPIOs |
| --- | --- |
| I2C | SCL 8, SDA 7 |
| I2S / ES8311 | SCLK 12, MCLK 13, LRCLK 10, DOUT 9, DSIN 11, amplifier enable 53 |
| LCD control | MOSI 20, clock 21, CS 23, D/C 26, reset 27, backlight 28 |
| Touch | reset 29, interrupt 50 |
| SDMMC | D0–D3 39–42, CMD 44, CLK 43 |

Treat these assignments, the display color order, and the selected dependency
profile as review-sensitive. A component update must not silently rewrite them.

## Dependency update rules

1. Prefer a bounded compatible range for ordinary managed libraries and an
   exact version where cross-version APIs or prebuilt ABI compatibility matter.
2. Keep the explicit ESP-IDF 5.5/6.0 conditions for TinyUSB, `esp_video`, and
   `esp_h264`; the examples use different feature generations and must not be
   unified merely because a newer version exists.
3. Update one dependency family at a time and record the reason in the manifest.
4. Keep Example 12 `espressif/esp_lvgl_port` at `==2.8.0~1` until the CI matrix
   moves beyond ESP-IDF 6.0 or its DPI callback API migration is complete;
   `2.9.0` requires the later callback API.
5. Use the post-commit GitHub Actions matrix as compile evidence. A green build
   does not replace display, touch, camera, audio, storage, USB, or hosted-Wi-Fi
   validation on physical hardware.
