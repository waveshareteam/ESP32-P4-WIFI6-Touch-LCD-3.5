# SD-card video player

[简体中文](README_ZH.md)

| Supported target | Board |
| --- | --- |
| ESP32-P4 | Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 |

This example reads a media file from the MicroSD card, decodes JPEG/MJPEG video
frames with the ESP32-P4 hardware JPEG decoder, draws RGB565 frames on the
built-in 320 × 480 LCD, and plays supported audio through the onboard ES8311
codec. No HDMI bridge or ESP32-P4-Function-EV-Board is required.

## Media requirements

- The default file is `/sdcard/test_video.mp4`.
- Change the file name in `Product LCD Media Player Configuration` → `Video File
  Configuration`; the mount point remains `/sdcard` unless the BSP setting is
  changed.
- The embedded extractor registers MP4 and AVI containers. The video path
  expects JPEG/MJPEG frames and converts decoded output to RGB565.
- Audio is enabled when the ES8311 codec initializes and the container exposes a
  supported audio stream. If codec initialization fails, video playback
  continues without audio.

An Espressif
[`test_video.mp4`](https://dl.espressif.com/AE/esp-dev-kits/test_video.mp4)
file can be used as a starting point. Media compatibility and sustained playback
must still be checked on the physical board; container recognition alone does
not prove that every codec combination is supported.

## Prepare the board

1. Format a MicroSD card with a filesystem supported by ESP-IDF FATFS.
2. Copy the selected file to the root of the card. With defaults, name it
   `test_video.mp4`.
3. Insert the card before powering or resetting the board.
4. Connect USB-UART for programming and logs.

The BSP mounts SDMMC slot 0 in 4-bit high-speed mode. Format-on-mount-failure is
disabled by default, so a missing or unreadable card is reported instead of
being reformatted.

## Configure, flash, and monitor

From this example directory:

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

The player repeatedly restarts the selected file after it reaches the end. Press
`Ctrl-]` to leave the monitor.

See the repository [CI policy](../../../docs/ci.md) for the ESP-IDF versions
compiled by Actions and the hardware-validation boundary.
