# ESP-IDF 示例

[English](README.md)

每个包含根 `CMakeLists.txt` 的直接子目录都是独立的第一方 ESP-IDF 工程，目标为
`esp32p4`。请一次打开和构建一个工程；本目录本身不是 ESP-IDF 应用。

| 示例 | 用途 |
| --- | --- |
| [01_HowToCreateProject](01_HowToCreateProject/) | 最小工程结构 |
| [02_HelloWorld](02_HelloWorld/) | 基础启动与系统信息 |
| [03_i2c_tools](03_i2c_tools/) | I2C 命令工具与发现 |
| [04_wifistation](04_wifistation/) | 通过 ESP32-C6 使用 hosted Wi-Fi |
| [05_sdmmc](05_sdmmc/) | MicroSD 访问 |
| [06_I2SCodec](06_I2SCodec/) | ES8311 录音和播放 |
| [07_Displaycolorbar](07_Displaycolorbar/) | LCD 初始化和彩条 |
| [08_lvgl_demo_v9](08_lvgl_demo_v9/) | LVGL 9 显示/触摸示例 |
| [09_video_lcd_display](09_video_lcd_display/) | 在 LCD 显示 OV5647 画面 |
| [10_mp4_player](10_mp4_player/) | SD 卡媒体播放 |
| [11_esp_brookesia_phone](11_esp_brookesia_phone/) | 手机风格本地 UI |
| [12_esp32-p4-eye](12_esp32-p4-eye/) | 摄像、相册、USB 与视觉示例 |

仓库分类器会为单工程源码修改只选择受影响工程，为共享或 CI 修改选择全部工程。详情见
[CI 策略](../../docs/ci_ZH.md)与[组件策略](../../docs/components_ZH.md)。

标准 CI 矩阵为 12 个工程 × ESP-IDF v5.5.5/v6.0.2 = 24 次构建，且全部使用 `rev1_3`
（v3 前）配置；不会按芯片版本将每个示例矩阵翻倍。维护的产品源码 `12_esp32-p4-eye`
在 IDF v6.0.2 上具有独立的 `rev1_3` 和 `rev3_x` 产品任务/制品。不同配置使用独立
sdkconfig/构建目录，二进制互不兼容。当前没有 Arduino 示例或 Arduino CI 构建；未来默认
Arduino 策略为 `ChipVariant=prev3`。编译通过不是硬件在环或外设验证。
