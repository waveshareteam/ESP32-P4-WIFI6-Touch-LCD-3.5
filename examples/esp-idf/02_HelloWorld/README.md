# Hello world and system information

[简体中文](README_ZH.md)

This example prints `Hello world!`, the detected target and core count, silicon
revision, flash size/type, and minimum free heap. It then counts down for ten
seconds and restarts the chip.

```text
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

No product peripheral, network credential, or storage device is required. A
pytest smoke test checks the greeting; additional test targets support the Linux
host and ESP32 QEMU paths inherited from the ESP-IDF example.
