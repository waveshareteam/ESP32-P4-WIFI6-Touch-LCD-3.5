# MicroSD read/write test

[简体中文](README_ZH.md)

This example mounts the product MicroSD slot at `/sdcard` through 4-bit SDMMC,
prints card information, and performs create/read/rename/delete checks. The
ESP32-P4 defaults match the product schematic: D0–D3 GPIO 39–42, CMD GPIO 44,
and CLK GPIO 43, with the internal SDMMC LDO on channel 4.

> [!WARNING]
> The default configuration enables format-on-mount-failure. The example also
> writes, renames, and deletes test files. Back up the card and do not use media
> containing valuable data.

The default bus limit is 26 MHz. A separate `FORMAT_SD_CARD` option can format
the card as an explicit test and is even more destructive.

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

Insert the card before running the test. A successful Actions build does not
validate card signal integrity, filesystem compatibility, or sustained I/O.
