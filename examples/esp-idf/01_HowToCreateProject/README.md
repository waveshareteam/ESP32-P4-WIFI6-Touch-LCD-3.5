# Minimal project skeleton

[简体中文](README_ZH.md)

This is the smallest project layout in the repository: a root `CMakeLists.txt`,
a `main` component, and an empty `app_main()`. It is intended as a clean starting
point for a new ESP-IDF application, not as a hardware demonstration.

```text
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

The unmodified application produces no serial output and initializes no board
peripherals. Add dependencies and BSP initialization deliberately as the project
grows. Repository CI checks this skeleton with ESP-IDF v5.5.5 and v6.0.2.
