# ESP-IDF examples

[简体中文](README_ZH.md)

Each direct child directory with a root `CMakeLists.txt` is an independent
first-party ESP-IDF project targeting `esp32p4`. Open and build one project at a
time; do not treat this directory itself as an ESP-IDF application.

| Example | Purpose |
| --- | --- |
| [01_HowToCreateProject](01_HowToCreateProject/) | Minimal project structure |
| [02_HelloWorld](02_HelloWorld/) | Basic startup and system information |
| [03_i2c_tools](03_i2c_tools/) | I2C command tools and discovery |
| [04_wifistation](04_wifistation/) | Hosted Wi-Fi through the ESP32-C6 |
| [05_sdmmc](05_sdmmc/) | MicroSD access |
| [06_I2SCodec](06_I2SCodec/) | ES8311 recording and playback |
| [07_Displaycolorbar](07_Displaycolorbar/) | LCD bring-up and color bars |
| [08_lvgl_demo_v9](08_lvgl_demo_v9/) | LVGL 9 display/touch demo |
| [09_video_lcd_display](09_video_lcd_display/) | OV5647 camera on the LCD |
| [10_mp4_player](10_mp4_player/) | SD-card media playback |
| [11_esp_brookesia_phone](11_esp_brookesia_phone/) | Phone-style local UI |
| [12_esp32-p4-eye](12_esp32-p4-eye/) | Camera, album, USB, and vision demo |

The repository classifier selects only affected projects for local source
changes and all projects for shared or CI changes. See the
[CI policy](../../docs/ci.md) and [component policy](../../docs/components.md).

The standard CI matrix is 12 projects × ESP-IDF v5.5.5/v6.0.2 = 24 builds,
all using the `rev3_x` (rev3.x) profile; it is not a doubled per-revision
matrix. The maintained product source `12_esp32-p4-eye` has separate
`rev1_3` and `rev3_x` product jobs/artifacts on IDF v6.0.2. Profiles have
independent sdkconfigs/build directories and incompatible binaries. Fresh local
configurations also default to `rev3_x`; select `rev1_3` explicitly only for
confirmed rev1.x silicon. There are
no Arduino examples or Arduino CI builds; the default future Arduino policy is
`ChipVariant=postv3`. Compile success is not HIL or peripheral validation.
