# Camera video on the product LCD

[简体中文](README_ZH.md)

| Supported target | Board |
| --- | --- |
| ESP32-P4 | Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 |

This example captures frames from the board's MIPI-CSI camera through the
`esp_video` V4L2 device, crops and scales them with the ESP32-P4 PPA, and draws
RGB565 stripes on the built-in 320 × 480 LCD. It does not target the
ESP32-P4-Function-EV-Board or an external 7-inch display.

## Requirements

- ESP-IDF 5.4 or later. Repository CI currently checks v5.5.5 and v6.0.2.
- ESP32-P4-WIFI6-Touch-LCD-3.5 with its OV5647 MIPI-CSI camera connected.
- A USB cable connected to the board's USB-UART port for programming and logs.

The default configuration selects OV5647 RAW10 at 1280 × 960 and 45 fps. Camera
availability, frame rate, and image quality still depend on the connected module
and must be verified on hardware.

## Data path and memory

The application allocates two camera USERPTR buffers and two LCD frame buffers
in PSRAM. A DMA-capable internal-memory stripe is used for the PPA-to-LCD copy.
The frame callback center-crops the camera aspect ratio to 320 × 480, applies a
horizontal mirror, scales into RGB565, swaps bytes for the display path, and
submits the result to the BSP display adapter.

The V4L2 helper accepts two or three frame buffers. Stream startup now fails
cleanly if `VIDIOC_STREAMON`, task creation, the buffer index, or the registered
frame callback is invalid.

## Configure, flash, and monitor

From this example directory:

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

Use the camera-sensor configuration menu only when changing the connected
sensor or its format. Do not apply SC2336 or Function-EV wiring instructions to
this product. Press `Ctrl-]` to leave the serial monitor.

See the repository [CI policy](../../../docs/ci.md) for the versions compiled by
Actions and the difference between compile evidence and board-level validation.
