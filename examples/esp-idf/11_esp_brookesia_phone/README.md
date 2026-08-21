# ESP-Brookesia phone-style UI

[简体中文](README_ZH.md)

| Supported target | Board |
| --- | --- |
| ESP32-P4 | Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 |

This example runs the vendored ESP-Brookesia Phone framework and a local
SquareLine application on the product's built-in 320 × 480 touch display. The
application selects the 320 × 480 stylesheet branch at startup and registers UI
pages for splash, clock, weather, calls, chat, alarms, and music controls.

This is a local UI demonstration. The default configuration disables the AI
framework, animation player, services, and speaker feature, and `main.cpp` does
not mount the SD card or initialize Wi-Fi, network credentials, or the audio
codec. The visible pages therefore do not imply that cloud, telephony, media, or
voice services are implemented.

## Requirements

- ESP-IDF 5.3 or later. Repository CI currently checks v5.5.5 and v6.0.2.
- ESP32-P4-WIFI6-Touch-LCD-3.5.
- A USB cable connected to USB-UART for programming and logs.

The required ESP-Brookesia components and SquareLine-generated application are
already included under this project. Do not clone a second `esp-brookesia`
repository into the example.

## Configure, flash, and monitor

From this example directory:

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

Changing the display resolution, LVGL profile, or BSP requires coordinated
updates to the stylesheet selection and the project-local BSP. See the
[component policy](../../../docs/components.md) before replacing the local BSP.
Press `Ctrl-]` to leave the serial monitor.

The repository [CI policy](../../../docs/ci.md) records the compiled framework
versions and explains why a green build is not a touch/display runtime test.
