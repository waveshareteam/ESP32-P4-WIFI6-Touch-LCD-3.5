# LVGL 9 benchmark

[简体中文](README_ZH.md)

This example starts the product display/touch BSP through the LVGL adapter,
turns on the backlight, and runs `lv_demo_benchmark()` by default. The project
pins `lvgl/lvgl` to the 9.4 series; the music and widgets demo calls remain
available in source but are not the default entry point.

Defaults target ESP32-P4 with 16 MB flash, 200 MHz PSRAM, the FreeRTOS LVGL OS
layer, a 15 ms refresh period, and two LVGL draw units.

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

The benchmark intentionally exercises rendering. Measure frame rate, touch
coordinates, color order, memory use, and temperature on the physical board;
compile success alone cannot establish display performance. See the
[component policy](../../../docs/components.md) before changing the BSP or LVGL
integration profile.
