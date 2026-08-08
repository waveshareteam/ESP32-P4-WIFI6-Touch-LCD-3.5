# LCD color-bar bring-up

[简体中文](README_ZH.md)

This example initializes the project-local product BSP, turns on the backlight,
and fills the 320 × 480 LCD with eight horizontal color bands: red, green, blue,
yellow, magenta, cyan, white, and black.

It is the smallest display-path test in the repository and is useful for
checking LCD reset, SPI control, backlight, pixel format, and color order before
adding LVGL. The relevant product pins are LCD MOSI 20, clock 21, CS 23, D/C 26,
reset 27, and backlight 28.

```text
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

Compare the visible order and color channels with the list above. A green CI
build cannot detect a disconnected panel, incorrect backlight, or red/blue swap.
Read the [component policy](../../../docs/components.md) before replacing the
local BSP with the Registry component.
