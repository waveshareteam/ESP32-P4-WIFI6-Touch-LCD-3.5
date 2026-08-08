# ESP32-P4 Eye camera and vision demo

[简体中文](README_ZH.md)

This product application combines the OV5647 MIPI-CSI camera, 320 × 480 UI,
photo/video capture, interval capture, an album, USB mass storage, and on-device
pedestrian or face detection. The default page is the main menu and the default
AI mode is pedestrian detection.

The UI contains entries for camera, interval capture, video, AI detection,
album, USB disk, and settings. A visible entry is not proof that its peripheral
is ready: startup logs report failures independently.

## Runtime requirements

- ESP32-P4-WIFI6-Touch-LCD-3.5 with an available MIPI-CSI camera device.
- A MicroSD card for saved photos/video, album indexing, and USB MSC export.
- GPIO 35 for the single-button control path used by this BSP profile: single
  click = OK, double click = down, triple click = up, long press = menu.
- ESP-IDF 5.3 or later. Repository CI checks v5.5.5 and v6.0.2 with conditional
  TinyUSB, `esp_video`, `esp_h264`, and ESP-DL compatibility logic.

Storage is monitored at runtime. When a card mounts, the application prepares
`/sdcard/esp32_p4_pic_save`, initializes the album, and installs USB MSC storage;
removing the card unmounts those services. Back up the card before using USB
mass-storage or capture features.

The application initializes pedestrian and face detection paths. Although a
COCO model component and UI resources are present, the audited runtime selector
does not enable a COCO detection mode. The QMA6100 source is also excluded from
the build, so this configuration must not be described as providing an active
IMU feature.

## Configure, flash, and monitor

From this example directory:

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

The default MIPI-CSI Kconfig uses SCCB I2C port 0 with SCL GPIO 34 and SDA GPIO
31 for the camera sensor. Confirm the camera module and board revision before
changing those values.

The IDF 6 compatibility layer also validates the expected TinyUSB and ESP-DL
source layouts at configure time. See the [component policy](../../../docs/components.md)
and [CI policy](../../../docs/ci.md). Actions compile evidence does not replace
camera, SD, USB, button, display, or AI runtime validation.
