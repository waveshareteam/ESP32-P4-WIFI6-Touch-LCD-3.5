# Arduino Examples

[简体中文](README_ZH.md)

These are the ten first-party Arduino sketches for
ESP32-P4-WIFI6-Touch-LCD-3.5. They cover the board's display, touch, camera,
storage, and audio functions. A successful compilation is not a hardware test;
test the intended sketch on the intended board.

## Install and configure

1. Install Arduino-ESP32 **3.3.11** in Arduino IDE or Arduino CLI.
2. Install **GFX Library for Arduino 1.6.7**. Install **LVGL 9.3.0** as well
   before building `04_LVGLV9_Arduino`.
3. Install the bundled `Waveshare_LCD35` library from this directory's
   [`libraries/`](libraries/) folder into the Arduino sketchbook libraries
   folder, or supply that folder as the additional Arduino library search path.
4. Select the ESP32-P4 board and choose `ChipVariant=postv3` for the default
   rev3.x silicon configuration. Select `ChipVariant=prev3` only for a chip
   confirmed as rev1.x, including rev1.3.

The silicon variant is not a PCB revision selection. See the
[silicon revision guide](../../docs/revisions.md) for the matching ESP-IDF
profile and MIPI DSI clock rule.

## Sketches

| Sketch | Purpose |
| --- | --- |
| [01_HelloWorld](examples/01_HelloWorld/) | ST7796 color bars and text. |
| [02_AsciiTable](examples/02_AsciiTable/) | ST7796 ASCII character table. |
| [03_Drawing_board](examples/03_Drawing_board/) | ST7796 SPI display drawing with polling touch input. |
| [04_LVGLV9_Arduino](examples/04_LVGLV9_Arduino/) | LVGL 9 display and polling touch example. |
| [05_GFX_ESPWiFiAnalyzer](examples/05_GFX_ESPWiFiAnalyzer/) | Wi-Fi scan display using GFX. |
| [06_Camera_Preview](examples/06_Camera_Preview/) | OV5647 camera preview on the ST7796 display. |
| [07_Camera_ISP_Tuning](examples/07_Camera_ISP_Tuning/) | OV5647 image-sensor controls for ISP tuning. |
| [08_SD_Card](examples/08_SD_Card/) | MicroSD card access. |
| [09_Audio_Playback](examples/09_Audio_Playback/) | ES8311 audio playback. |
| [10_Mic_Record](examples/10_Mic_Record/) | ES8311 microphone capture. |

## Display, camera, and touch boundaries

The supplied display is a 320 × 480 ST7796 panel on SPI at 80 MHz; it is not a
MIPI DSI panel. The OV5647 is connected through MIPI-CSI. Do not apply a MIPI
DSI PHY clock choice to either of those paths. If adapting a sketch for an
external DSI panel, use the DSI panel's validated timing and select `PLL_F20M`
for rev1.x or `XTAL` for rev3.x as described in the
[silicon revision guide](../../docs/revisions.md).

The touch helper first probes GT911 at I2C `0x5D`, then `0x14`, and initializes
the driver using the address that acknowledges. It leaves `INT` and `RST`
unspecified and polls for touch data. The released LCD-3.5 board uses an
FT6336/FT5x06-compatible touch controller, so the helper falls back to I2C
`0x38` only when neither GT911 address responds. Confirm the actual controller
on a target board before treating any touch result as valid.

The schematic has no onboard CAN or RS-485 transceiver. An external CAN or
RS-485 PHY may be designed into an application separately, but these sketches
do not claim onboard support for either interface.
