# Component Maintenance Policy

[简体中文](components_ZH.md)

This repository uses the public managed board-support package for the single
product and retains only product-specific glue where an application needs it.

## Current classification

| Component | Location | Decision |
| --- | --- | --- |
| ESP32-P4-WIFI6-Touch-LCD-3.5 BSP | Product examples | Managed dependency `waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.1` |
| `bsp_extra` | Product examples where present | Retain only as product-specific glue |
| `sd_card` | Example 05 | Keep as example test support |
| `esp_painter` | Example 12 | Keep as product UI/rendering support |
| `esp_extractor` | Examples 10 and 11 | Keep embedded with its target-specific prebuilt library |
| Detection model wrappers | Example 12 | Keep locally; consume `espressif/esp-dl ==3.1.3` through Component Manager |
| `espressif/button` | Example 12 | Managed and pinned to `==4.2.0` |
| LVGL runtime | Example 12 | The managed BSP pulls `esp_lvgl_adapter`; Example 12 directly pins `lvgl/lvgl` to `8.3.*` for its 21 SquareLine-generated image assets |

The six copied local BSP component variants have been removed. The public
dependency is exactly
[`waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.1`](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_3_5/versions/2.0.1/readme).
The Registry release resolves to the official immutable source commit
`3bbbaa429bc719b80c4a367ea2a30f217aa727dc`, which was audited for the
migration; product manifests and public dependency references use the Registry
package and version above.

As of this repository revision, 2.0.1 is published in Component Registry and
is the version pinned by every product manifest. A merged component or BSP change is not a product dependency
until the matching Registry release is publicly available. Do not replace the
version with a Git URL, branch, commit, local path, or an unpublished version:
those forms are unsuitable for a publishable product manifest and may be
rejected by Component Registry CI.

## Managed BSP boundary

Do not restore a copied board-support package merely to make a local variation
of a shared BSP. Keep `bsp_extra` only when its code is demonstrably
product-specific application glue; the common board support remains the managed
Registry dependency. Compile evidence does not establish hardware behavior.
Example 12's legacy flashlight, card-presence, and sleep helpers retain their
pre-migration inert or `SDMMC_SLOT_NO_CD` behavior because the managed BSP does
not expose corresponding GPIO-backed APIs. They are compatibility shims, not a
claim that those hardware controls are implemented.

### Display configuration contract

The official BSP 2.0.1 immutable public header fixes the display to RGB565,
big-endian color order, and 16 bits per pixel. Example 09 therefore owns an
unconditional RGB565 video-format contract instead of selecting a removed BSP
Kconfig color-format option. Example 10 owns `APP_LCD_BUFFER_COUNT` fixed at 2
instead of a removed DPI-buffer Kconfig option. These source contracts do not
claim hardware-in-the-loop validation.

Registry BSP 2.0.1 also declares `esp_err_t` from `bsp/display.h` without
including `esp_err.h`. Example 12 therefore includes `esp_err.h` before that
public header in its three direct consumers. The separate
[BSP 2.0.2 pull request](https://github.com/waveshareteam/Waveshare-ESP32-components/pull/203)
makes the header self-contained. The product must remain on 2.0.1 until 2.0.2
is published in Component Registry; only then may the workaround be removed
after validation.

## Dependency update rules

1. Prefer a bounded compatible range for ordinary managed libraries and an
   exact version where cross-version APIs or prebuilt ABI compatibility matter.
2. Keep explicit ESP-IDF compatibility conditions where a product application
   requires them; do not turn a component update into a hardware-validation
   claim.
3. Update one dependency family at a time and record the reason in the manifest.
4. Keep Example 12 on `lvgl/lvgl 8.3.*` while its 21 SquareLine-generated image
   assets use the LVGL 8 descriptor contract. The managed BSP supplies
   `esp_lvgl_adapter`; do not add an unused direct `esp_lvgl_port` dependency.
   BSP 2.0.1 exposes the matching LVGL 8 rotation type directly, so the former
   product-side forced-include compatibility shim has been removed. Migrating
   to LVGL 9 still requires regenerating and auditing the complete UI.
5. Keep Example 10's `espressif/esp_audio_codec` at `>=2.3.0,<2.6.0`: v2.6+
   requires ESP32-P4 revision 3 or newer, while `rev1_3` remains an explicit
   compatibility profile. This dependency constraint is not a hardware-validation
   claim.
6. Submit shared component changes first, then the BSP that consumes them, and
   only then a product manifest update after the required package is published
   to Component Registry. Keep these as independently reviewable pull requests.
   A temporary local component is acceptable only for local investigation; it
   must not be committed as a substitute for a Registry release.
7. The Arduino board library is source bundled under `examples/arduino/`; it
   does not change or shadow the managed BSP dependency used by ESP-IDF examples.
8. Use the post-commit GitHub Actions matrix as compile evidence. A green build
   is not HIL evidence and does not replace physical-hardware validation.
