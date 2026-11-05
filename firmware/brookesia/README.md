# ESP-Brookesia Factory Firmware

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5%2B-E7352C?logo=espressif)](https://github.com/espressif/esp-idf/releases)
[![Target](https://img.shields.io/badge/target-ESP32--P4-1F6FEB)](https://www.espressif.com/en/products/socs/esp32-p4)
[![LVGL](https://img.shields.io/badge/LVGL-9.5.0-2AA9E0?logo=lvgl)](https://lvgl.io/)

English | [简体中文](README_ZH.md)

This directory contains the ESP-Brookesia firmware source for the
**Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5**. It drives the 3.5-inch 320 × 480 SPI
touch display, the ES8311 audio codec, the OV5647 camera, the microSD card and
the ESP32-C6 hosted Wi-Fi.

## Platform configuration

| Item | Configuration |
| --- | --- |
| Board | Waveshare [ESP32-P4-WIFI6-Touch-LCD-3.5](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.5.htm) |
| Host MCU | ESP32-P4NRW32, 16 MB flash, 32 MB PSRAM |
| Display / touch | 3.5-inch 320 × 480 portrait IPS, ST7796 over SPI; FT6336 capacitive touch over I2C |
| Framework | ESP-IDF v5.5 or later (`main/idf_component.yml`) |
| UI | LVGL 9.5.0 with the local ESP-Brookesia core 0.6.0-beta2; `espressif/esp_lvgl_adapter` ^0.7.3~1 |
| Board support | `waveshare/waveshare_boards` ^0.2.0 (board `esp32_p4_wifi6_touch_lcd_3_5`) on `espressif/esp_board_manager` ^0.7.5, wrapped by the local [`components/board`](components/board/) |
| Wireless | ESP32-C6 over SDIO using ESP-Hosted (`espressif/esp_hosted` ^1.4.7, `esp_wifi_remote` 0.14.5) and `esp_wifi_service` ^0.5.3 |
| Audio | ES8311 codec with the onboard microphone and speaker |
| Camera | OV5647 over MIPI-CSI with `espressif/esp_video` ^2.3.0 |
| Storage | microSD card (4-bit SDMMC) and the SPIFFS `storage` partition |
| Voice assistant | `espressif/esp_xiaozhi` 0.1.3~1, ESP-SR 2.4.7 and xiaozhi-fonts 1.6.0 |
| Media | `espressif/esp_extractor` 1.0.7 and `esp_new_jpeg` 1.0.2 |

See the [schematic](../../schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf)
and the [product documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-3.5)
for interfaces and pin assignments. The ESP32-P4 silicon revision options are in
`sdkconfig.defaults`; read the [revision guide](../../docs/revisions.md) before
changing them.

## Applications

<table>
  <tr>
    <td align="center"><img src="components/draw/assets/img_app_drawpanel.png" width="64" alt="DrawPanel icon"><br><b>DrawPanel</b></td>
    <td align="center"><img src="components/brookesia_app_squareline_demo/assets/esp_brookesia_app_icon_launcher_squareline_112_112.png" width="64" alt="Squareline icon"><br><b>Squareline</b></td>
    <td align="center"><img src="components/brookesia_app_calculator/assets/img_app_calculator.png" width="64" alt="Calculator icon"><br><b>Calculator</b></td>
    <td align="center"><img src="components/SpecAnalyzer/assets/img_app_specanalyzer.png" width="64" alt="SpecAnalyzer icon"><br><b>SpecAnalyzer</b></td>
    <td align="center"><img src="components/MusicPlayer/assets/img_app_musicplayer.png" width="64" alt="MusicPlayer icon"><br><b>MusicPlayer</b></td>
  </tr>
  <tr>
    <td align="center"><img src="components/Camera/assets/img_app_camera.png" width="64" alt="Camera icon"><br><b>Camera</b></td>
    <td align="center"><img src="components/VideoPlayer/assets/img_app_vedioplayer.png" width="64" alt="VideoPlayer icon"><br><b>VideoPlayer</b></td>
    <td align="center"><img src="components/Settings/assets/img_app_settings.png" width="64" alt="Settings icon"><br><b>Settings</b></td>
    <td align="center"><img src="components/XiaozhiApp/assets/img_app_xiaozhi.png" width="64" alt="AIChats icon"><br><b>AIChats</b></td>
    <td></td>
  </tr>
</table>

| Application | Function | Runtime dependency |
| --- | --- | --- |
| DrawPanel | Drawing in a fixed canvas area without dragging the entire app at the edge | Display and touch |
| Squareline | Registry-installed Squareline Studio UI demo and application template | Display and touch |
| Calculator | Touch-friendly Brookesia calculator | Display and touch |
| SpecAnalyzer | Single-channel FFT spectrum from the on-board microphone | ES8311 input |
| MusicPlayer | Plays the MP3 files in `/spiffs/music` with a spectrum visualizer | SPIFFS `storage`, ES8311 output |
| Camera | OV5647 MIPI CSI preview, scaled by the PPA to fill the panel | OV5647, `esp_video` |
| VideoPlayer | Full-screen AVI/MJPEG playback from the SD card with PCM audio; see [Camera and media](#camera-and-media) | SD card, JPEG/PPA, ES8311 output |
| Settings | WLAN scan/connect, sound, display brightness, SNTP time and device information; brightness and volume are kept in NVS | ESP32-C6 hosted Wi-Fi |
| AIChats | Network check, Xiaozhi activation, WakeNet wake word, VAD, Opus uplink/downlink, server transcript and TTS | Wi-Fi, `model`/`storage` images, ES8311 |

## Build

### Requirements

Use ESP-IDF v5.5 or later with the `esp32p4` toolchain. Network access to the
ESP Component Registry is required. Use a data-capable USB cable for flashing.

All commands below run from this directory. Starting from the repository root:

```sh
cd firmware/brookesia
idf.py --version
idf.py set-target esp32p4
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_3_5
idf.py build
```

`set-target` downloads the managed components, including the board definition
from `waveshare/waveshare_boards`. `bmgr` generates `components/gen_bmgr_codes`
for the selected board. That directory is git-ignored; run `bmgr` again after
changing the board or updating `waveshare_boards`.

### First full flash and partition changes

The first installation must write the bootloader, partition table, application,
ESP-SR model and SPIFFS resources:

```sh
idf.py -p PORT flash monitor
```

Erase the flash before a full flash when changing partition offsets or sizes,
upgrading from an older partition layout, or investigating stale model or
filesystem images:

```sh
idf.py -p PORT erase-flash flash monitor
```

When the partition layout, voice model and `spiffs/` resources are unchanged,
you can flash only the application:

```sh
idf.py -p PORT app-flash monitor
```

`app-flash` does not update `srmodels.bin`, `storage.bin` or the partition
table. Run a full `flash` after changing `partitions.csv`, WakeNet/model Kconfig,
xiaozhi-fonts, bundled music or prompt audio.

### Flash partitions

The 16 MB layout is defined in `partitions.csv`. The bootloader is at `0x2000`
and the partition table at `0x9000`:

| Name | Type | Offset | Size | Purpose |
| --- | --- | --- | --- | --- |
| `nvsfactory` | data / nvs | `0xA000` | 200 KiB | Factory NVS data |
| `nvs` | data / nvs | `0x3C000` | 840 KiB | Wi-Fi, app settings and Xiaozhi state |
| `otadata` | data / ota | `0x10E000` | 8 KiB | OTA data (reserved) |
| `phy_init` | data / phy | `0x110000` | 4 KiB | PHY calibration data |
| `model` | data / spiffs | `0x111000` | 956 KiB | ESP-SR speech models (`srmodels.bin`) |
| `factory` | app / factory | `0x200000` | 8 MiB | Application |
| `storage` | data / spiffs | `0xA00000` | 6 MiB | Music, Xiaozhi prompts and font (`storage.bin`) |

## AIChats / Xiaozhi

AIChats uses `esp_xiaozhi` for the protocol. This project provides the UI,
activation, audio and lifecycle integration.

First use:

1. Configure WLAN in Settings.
2. Open AIChats. If the device is not associated with an account, it displays
   and announces an activation code.
3. Add the device in the [Xiaozhi control panel](https://xiaozhi.me) and enter
   the activation code.
4. When the page reaches standby, say the wake phrase. The default is
   "Ni Hao Xiao Zhi."

If the log reports `No speech model found` or `contains no compatible models`,
rebuild and perform a full flash to write both the partition table and
`srmodels/srmodels.bin`.

The local UI language and the preferred transport can be selected in this menu:

```text
idf.py menuconfig
  Xiaozhi Application
    Local UI language
      English
      Simplified Chinese
    Preferred Xiaozhi transport
      WebSocket
      Auto (prefer MQTT+UDP)
      MQTT+UDP
```

`sdkconfig.defaults` selects English and Auto; the Kconfig default transport is
WebSocket. The language option controls firmware-owned text only. It does not
translate conversation text, TTS or activation messages returned by the server.
Local strings are in `components/XiaozhiApp/XiaozhiLocalization.*`.

The board has one microphone on the ES8311. AIChats and SpecAnalyzer capture a
single channel; AIChats converts it to 16 kHz mono Opus for uplink, and the
playback chain converts server audio as needed for the codec output.

## Camera and media

Camera runs the OV5647 in its 800 × 640 binned mode. The PPA scales the frame
and center-crops it to fill the 320 × 480 panel; the scale factor is rounded to
the PPA's 1/16 steps and the crop is derived from it.

MusicPlayer plays the MP3 files in `/spiffs/music`. After changing tracks,
regenerate and flash `storage.bin`; `app-flash` alone does not update it.

VideoPlayer scans only `/sdcard/avi`; the `.avi` extension is case-insensitive.
AVI files in the SD card root and other containers such as MP4 or MOV are not
listed. It demuxes AVI with `esp_extractor`, decodes MJPEG with the hardware
JPEG decoder (falling back to `esp_new_jpeg` for frames the hardware cannot
decode) and plays PCM audio; other audio formats are skipped. The PPA crops and
scales each frame to fill the panel, and the frame then goes to the ST7796 over
SPI in 32-line bands. The 80 MHz SPI clock limits a full frame to about 30 fps;
frames that fall behind the audio are dropped.

## Development notes

- Apps register themselves with `ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR`.
  `BOOT_APP_GROUPS` in `main/main.cpp` sets only the home-screen order and the
  boot stage shown while each group installs; registered apps not listed there
  are installed after the last group.
- New apps derive from `esp_brookesia::systems::phone::App` and implement
  `run()` and `back()`. Override other lifecycle functions according to
  resource ownership. Each component declares its own dependencies in its
  `idf_component.yml`; `main` keeps only the ones it uses directly.
- Hardware access goes through [`components/board`](components/board/)
  (`board_display`, `board_audio`, `board_camera`, `board_storage`,
  `board_wifi`), which is built on `esp_board_manager`. Apps call these
  `board_*` APIs instead of touching the drivers directly.
- The MCP tool entry point is `registerXiaozhiDeveloperTools()` in
  `components/XiaozhiApp/XiaozhiDeveloperTools.cpp`. Register only implemented
  tools that return real results; run long operations in an application task.
- Access LVGL objects on the LVGL thread or while holding the display lock
  briefly. On app shutdown, release its timers, tasks, queues, codec sessions,
  SD handles and LVGL objects.

## Notes

- Use the LVGL lock only for UI operations. Run networking, SD scans, audio
  initialization, NVS access and model loading outside the lock.
- Model, font, prompt, music and partition changes require a full `flash`.
- Audio apps share one physical bus. Stop or hand off the codec session when
  switching between apps.
- The host component must remain compatible with the coprocessor firmware on
  the ESP32-C6.

`EXAMPLE_SHOW_MEM_INFO` in `main/main.cpp` is currently `true`, so periodic
SRAM/PSRAM statistics are enabled. The detailed log output is controlled
separately by `EXAMPLE_SHOW_MEM_INFO_LOG`.

When reporting an issue, include the board revision, ESP-IDF version, complete
reproduction steps and the first actual error in the logs.

The repository is licensed under the [Apache-2.0 license](../../LICENSE). The
local Brookesia core has its own
[Apache-2.0 license file](components/brookesia_core/license.txt) that applies
only to that component, and bundled third-party code under
`components/XiaozhiApp/third_party/` keeps its own license.
