<div align="center">
  <h1>ESP32-P4-WIFI6-Touch-LCD-3.5</h1>
  <p><strong>ESP32-P4 3.5-inch 320 × 480 SPI LCD smart vision development board</strong></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/esp-idf.yml"><img alt="ESP-IDF Examples" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/esp-idf.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/docs.yml"><img alt="Documentation" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/docs.yml/badge.svg"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5"></a>
  </p>
  <p>
    <a href="README_ZH.md">中文</a> ·
    <a href="https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.5.htm">🌐 Product Page</a> ·
    <a href="https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-3.5">📚 Documentation</a> ·
    <a href="#-firmware">📦 Firmware</a> ·
    <a href="examples/esp-idf/">🧩 ESP-IDF Examples</a> ·
    <a href="examples/arduino/">🔧 Arduino Examples</a> ·
    <a href="schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf">🧾 Schematic</a>
  </p>
  <img src="assets/ESP32-P4-WIFI6-Touch-LCD-3.5-details-1.jpg" alt="Front and rear views of the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 development board" width="600">
</div>

---

## ✨ Overview

This repository provides first-party ESP-IDF examples, a bundled
firmware image, and the schematic for the Waveshare
ESP32-P4-WIFI6-Touch-LCD-3.5.

The board combines an ESP32-P4 application processor with an ESP32-C6 wireless
coprocessor, a 3.5-inch capacitive touch display, camera, audio, storage, and
power-management peripherals. It is designed for smart displays, voice and
vision interfaces, edge AI, and other interactive embedded applications.

## 🖥️ Hardware Overview

| Feature | Device / interface |
| --- | --- |
| Main processor | ESP32-P4NRW32, high-performance dual-core plus low-power single-core RISC-V |
| Memory | 32 MB in-package PSRAM and 16 MB external NOR flash |
| Wireless | ESP32-C6FH8 over SDIO, providing 2.4 GHz Wi-Fi 6 and Bluetooth 5 LE |
| Display | 3.5-inch 320 × 480 IPS LCD using the ST7796 controller over SPI |
| Touch | FT6336 capacitive touch controller over I2C |
| Camera | 5 MP OV5647 camera over MIPI-CSI |
| Audio | ES8311 codec, onboard microphone, and 8 Ω / 2 W speaker |
| Power | AXP2101 power management and a 3.7 V lithium battery connector |
| Storage | MicroSD card slot using SDIO 3.0 |
| USB | USB-UART Type-C and USB 2.0 High-Speed OTG Type-C |
| Hardware files | [Schematic](schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf) |

> [!NOTE]
> Wireless connectivity is provided by the onboard ESP32-C6 coprocessor; the
> ESP32-P4 itself does not contain a Wi-Fi or Bluetooth radio.

## 🚀 Getting Started

1. Read the official [product documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-3.5).
2. Install the ESP-IDF version required by the example you want to use.
3. Open a project under [`examples/esp-idf/`](examples/esp-idf/) and review its
   configuration files.
4. Set the target to `esp32p4`, then configure, build, flash, and monitor the
   selected project with ESP-IDF.

New example configurations default to the `rev3_x` ESP32-P4 silicon profile.
Use `rev1_3` only when a chip probe or other authoritative source confirms
rev1.x silicon (including rev1.3). The names identify silicon revision, not PCB
or product hardware revision; the profiles have incompatible binaries. See the
[CI guide](docs/ci.md#esp32-p4-revision-profiles) for the explicit profile
selection command and configuration differences.

See the [revision guide](docs/revisions.md) before changing a chip-variant or
MIPI DSI clock setting. Those choices are silicon-revision settings, not PCB
revision labels.

The Wi-Fi example communicates through the ESP32-C6 coprocessor. Keep the host
components and coprocessor firmware compatible when changing its dependencies.

## 🧪 ESP-IDF Examples

| Example | Focus |
| --- | --- |
| [01_HowToCreateProject](examples/esp-idf/01_HowToCreateProject/) | Minimal ESP-IDF project structure |
| [02_HelloWorld](examples/esp-idf/02_HelloWorld/) | Basic application and system information |
| [03_i2c_tools](examples/esp-idf/03_i2c_tools/) | I2C bus tools and device discovery |
| [04_wifistation](examples/esp-idf/04_wifistation/) | Wi-Fi station through the ESP32-C6 coprocessor |
| [05_sdmmc](examples/esp-idf/05_sdmmc/) | MicroSD card access |
| [06_I2SCodec](examples/esp-idf/06_I2SCodec/) | ES8311 audio input and output |
| [07_Displaycolorbar](examples/esp-idf/07_Displaycolorbar/) | LCD bring-up and color bars |
| [08_lvgl_demo_v9](examples/esp-idf/08_lvgl_demo_v9/) | LVGL 9 display and touch demo |
| [09_video_lcd_display](examples/esp-idf/09_video_lcd_display/) | OV5647 camera video on the LCD |
| [10_mp4_player](examples/esp-idf/10_mp4_player/) | MP4 playback with video and audio |
| [11_esp_brookesia_phone](examples/esp-idf/11_esp_brookesia_phone/) | ESP-Brookesia phone-style UI |
| [12_esp32-p4-eye](examples/esp-idf/12_esp32-p4-eye/) | Camera, album, and on-device vision demo |

## 🧪 Arduino Examples

[`examples/arduino/`](examples/arduino/) provides ten first-party sketches:
Hello World, ASCII table, drawing board, LVGL 9, Wi-Fi analyzer, camera preview,
camera ISP tuning, SD card, audio playback, and microphone recording. Install
Arduino-ESP32 3.3.11 with `ChipVariant=postv3` for the default rev3.x silicon
configuration. Use `ChipVariant=prev3` only for confirmed rev1.x silicon,
including rev1.3. The repository keeps the complete LCD-5 Arduino library
baseline, including GFX Library for Arduino 1.6.0, LVGL 9.3.0, the full LVGL
configuration, and the display/touch helpers. LCD-3.5 sketches use the bundled
`Waveshare_LCD35` board layer for the ST7796 hardware. See the
[Arduino guide](examples/arduino/README.md) for the board options, bundled
libraries, touch probing, and each sketch's scope.

| Sketch | Focus |
| --- | --- |
| [01_HelloWorld](examples/arduino/examples/01_HelloWorld/) | ST7796 color bars and text |
| [02_AsciiTable](examples/arduino/examples/02_AsciiTable/) | ST7796 ASCII character table |
| [03_Drawing_board](examples/arduino/examples/03_Drawing_board/) | Display drawing and touch |
| [04_LVGLV9_Arduino](examples/arduino/examples/04_LVGLV9_Arduino/) | LVGL 9 display and touch |
| [05_GFX_ESPWiFiAnalyzer](examples/arduino/examples/05_GFX_ESPWiFiAnalyzer/) | Wi-Fi scan display |
| [06_Camera_Preview](examples/arduino/examples/06_Camera_Preview/) | OV5647 preview |
| [07_Camera_ISP_Tuning](examples/arduino/examples/07_Camera_ISP_Tuning/) | OV5647 ISP controls |
| [08_SD_Card](examples/arduino/examples/08_SD_Card/) | MicroSD access |
| [09_Audio_Playback](examples/arduino/examples/09_Audio_Playback/) | Audio playback |
| [10_Mic_Record](examples/arduino/examples/10_Mic_Record/) | Microphone capture |

## ✅ Continuous Integration

The [ESP-IDF Examples workflow](.github/workflows/esp-idf.yml) dynamically
discovers every direct project under `examples/esp-idf/` and classifies the
complete Git diff. Project-local source changes build only the affected project;
shared, workflow, or unknown build inputs select all projects. Documentation and
governance changes still report the stable CI gate but skip the firmware matrix.
The standard matrix remains 12 projects × ESP-IDF v5.5.5/v6.0.2 = 24 builds,
all using the `rev3_x` profile for rev3.x silicon; it is not doubled for every silicon
revision. The maintained product source, `12_esp32-p4-eye`, additionally has
profile-qualified `rev1_3` and `rev3_x` product jobs/artifacts on IDF v6.0.2.
Those profiles use incompatible binaries and separate sdkconfigs/build
directories. See the [CI guide](docs/ci.md) for exact profile settings and
flashing safeguards.

Arduino CI separately discovers and compiles all ten Arduino sketches against
Arduino-ESP32 3.3.11 using the `postv3` default. A successful build remains
compile evidence only; it does not prove display, touch, camera, audio, SD, or
external-interface operation on a board.

Every successful matrix entry uploads a flashable artifact derived from that
project's `flasher_args.json`. The checked-in prebuilt image under `firmware/`
is intentionally excluded. See the [CI guide](docs/ci.md) for diff routing,
project selection, version updates, artifact contents, required-check behavior,
and validation boundaries.

## 📦 Firmware <a id="-firmware"></a>

[`firmware/ESP32-P4-WiFi6-LCD-3in5.bin`](firmware/ESP32-P4-WiFi6-LCD-3in5.bin)
is a checked-in prebuilt firmware image, not an ESP-IDF source project or a CI
build output. Follow the official product documentation for the supported
flashing procedure.

Source and build instructions for this prebuilt image are not included in this
repository yet and may be added in a later update. Source CI never repackages or
re-uploads this file; any replacement requires separate release provenance and
hardware-validation evidence.

## 🗂️ Repository Layout

| Path | Purpose |
| --- | --- |
| [`examples/esp-idf/`](examples/esp-idf/) | First-party ESP-IDF projects |
| [`examples/arduino/`](examples/arduino/) | First-party Arduino sketches and board library |
| [`firmware/`](firmware/) | Prebuilt firmware image |
| [`schematic/`](schematic/) | Product schematic |
| [`assets/`](assets/) | Product images used by the documentation |
| [`docs/`](docs/) | CI, component, revision, and maintenance policy |
| [`.github/workflows/`](.github/workflows/) | Example, product-firmware, repository-policy, and documentation CI |
| [`scripts/`](scripts/) | CI discovery, artifact packaging, and documentation validation helpers |

## 📚 Documentation and Support

- [Product Page](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.5.htm)
- [Product Documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-3.5)
- [Schematic](schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf)
- [CI and artifact policy](docs/ci.md)
- [Component maintenance policy](docs/components.md)
- [ESP32-P4 silicon revision guide](docs/revisions.md)
- [Contributing guide](CONTRIBUTING.md)
- [Support guide](SUPPORT.md)
- [Open an Issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/issues/new)

When reporting a problem, include the example path, ESP-IDF version,
reproduction steps, expected behavior, actual behavior, and relevant serial
logs. For product support, also provide the order number when contacting
Waveshare.

A green Actions run is compile-and-package evidence, not board-level proof.
Display, touch, camera, audio, storage, USB, power, and ESP32-C6 hosted-wireless
behavior must still be validated on the target hardware; this repository makes
no hardware-validation claim.

## 📄 License

This repository is licensed under the Apache License 2.0. See
[LICENSE](LICENSE).

Bundled third-party components remain subject to their respective licenses.
