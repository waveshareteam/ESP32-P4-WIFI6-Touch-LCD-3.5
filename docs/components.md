# Component Maintenance Policy

[简体中文](components_ZH.md)

This repository uses the public managed board-support package for the single
product and retains only product-specific glue where an application needs it.

## Current classification

| Component | Location | Decision |
| --- | --- | --- |
| ESP32-P4-WIFI6-Touch-LCD-3.5 BSP | Product examples | Managed dependency `waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.0` |
| `bsp_extra` | Product examples where present | Retain only as product-specific glue |
| `sd_card` | Example 05 | Keep as example test support |
| `esp_painter` | Example 12 | Keep as product UI/rendering support |
| `esp_extractor` | Examples 10 and 11 | Keep embedded with its target-specific prebuilt library |
| Detection model wrappers | Example 12 | Keep locally; consume `espressif/esp-dl ==3.1.3` through Component Manager |
| `espressif/button` | Example 12 | Managed and pinned to `==4.2.0` |
| LVGL runtime | Example 12 | The managed BSP pulls `esp_lvgl_adapter`; Example 12 directly pins `lvgl/lvgl` to `8.3.*` for its 21 SquareLine-generated image assets |

The six copied local BSP component variants have been removed. The public
dependency is exactly
[`waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.0`](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_3_5/versions/2.0.0/readme).
The Registry release resolves to the official immutable source commit
`a7c084c0425ef104f3ecf288f3afd1ff8ef4f97b`, which was audited for the
migration; product manifests and public dependency references use the Registry
package and version above.

## Managed BSP boundary

Do not restore a copied board-support package merely to make a local variation
of a shared BSP. Keep `bsp_extra` only when its code is demonstrably
product-specific application glue; the common board support remains the managed
Registry dependency. Compile evidence does not establish hardware behavior.
Example 12's legacy flashlight, card-presence, and sleep helpers retain their
pre-migration inert or `SDMMC_SLOT_NO_CD` behavior because the managed BSP does
not expose corresponding GPIO-backed APIs. They are compatibility shims, not a
claim that those hardware controls are implemented.

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
   Migrating to LVGL 9 requires regenerating and auditing the complete UI.
5. Use the post-commit GitHub Actions matrix as compile evidence. A green build
   is not HIL evidence and does not replace physical-hardware validation.
