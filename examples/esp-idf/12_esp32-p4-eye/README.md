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

## ESP32-P4 silicon-revision profiles

Use `rev1_3` for pre-v3 silicon, with
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and
`CONFIG_ESP32P4_REV_MIN_100=y`. Use `rev3_x` for v3-or-later silicon, with
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n` and
`CONFIG_ESP32P4_REV_MIN_300=y`. The profiles have independent sdkconfigs and
build directories; their binaries are incompatible.

On ESP-IDF v6.0.2, this maintained product source produces separate
profile-qualified jobs and artifacts for both profiles, bound to the PR branch's
final HEAD rather than a temporary merge commit. The host-only ESP32-P4 packages
contain no ESP32-C6 firmware, forbid explicit full-chip or region erase
operations, reject ranges beyond the 32 MiB artifact-policy ceiling, and
validate offsets, SHA-256 hashes, file sizes, and ESP32-P4 image chip ID 18; raw
partition, NVS, and data entries remain
valid. Normal `write_flash` may erase only the sectors it writes. The flasher
probes and re-probes silicon revision: below 3 accepts only `rev1_3`, and 3 or
later accepts only `rev3_x`. Silicon revision does not determine the PCB or
electrical revision.

The managed BSP uses `SDMMC_SLOT_NO_CD`, and this application's legacy presence
helper therefore treats the card as always present. Startup attempts to mount
the card, prepares `/sdcard/esp32_p4_pic_save`, initializes the album, and
installs USB MSC storage when mounting succeeds. Physical removal is not
detected or unmounted automatically; back up the card and power down the board
before removing it while storage, USB mass-storage, or capture features may be
active.

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
and [CI policy](../../../docs/ci.md). Actions compile evidence is not HIL and
does not replace camera, SD, USB, button, display, or AI runtime validation.
