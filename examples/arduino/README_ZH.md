# Arduino 示例

[English](README.md)

这里提供 ESP32-P4-WIFI6-Touch-LCD-3.5 的 10 个第一方 Arduino sketch，覆盖板载显示、
触摸、摄像头、存储和音频功能。编译成功不是硬件测试；请在目标板上测试对应 sketch。

## 安装与配置

1. 在 Arduino IDE 或 Arduino CLI 中安装 Arduino-ESP32 **3.3.11**。
2. 将随仓库提供的完整 [`libraries/`](libraries/) 目录作为一个整体保留。它是 LCD-5 的
   参考库基线，包含 **GFX Library for Arduino 1.6.0**、**LVGL 9.3.0**、完整
   `lv_conf.h` 以及显示和触摸源码。
3. 将整个 `libraries/` 目录加入 Arduino 库搜索路径，或把其中全部内容一起安装到
   sketchbook 的 libraries 目录。LCD-3.5 sketch 选择随仓库提供的 `Waveshare_LCD35`
   板级层。
4. 选择 ESP32-P4 开发板；默认 rev3.x 芯片配置选择 `ChipVariant=postv3`。只有确认芯片为
   rev1.x（含 rev1.3）时才选择 `ChipVariant=prev3`。

芯片变体不是 PCB 版本选择。对应的 ESP-IDF profile 和 MIPI DSI 时钟规则请参阅
[芯片版本说明](../../docs/revisions_ZH.md)。

## Sketch 列表

| Sketch | 功能 |
| --- | --- |
| [01_HelloWorld](examples/01_HelloWorld/) | ST7796 彩条与文字显示。 |
| [02_AsciiTable](examples/02_AsciiTable/) | ST7796 ASCII 字符表。 |
| [03_Drawing_board](examples/03_Drawing_board/) | 使用轮询触摸输入在 ST7796 SPI 显示屏上绘图。 |
| [04_LVGLV9_Arduino](examples/04_LVGLV9_Arduino/) | LVGL 9 显示与轮询触摸示例。 |
| [05_GFX_ESPWiFiAnalyzer](examples/05_GFX_ESPWiFiAnalyzer/) | 使用 GFX 显示 Wi-Fi 扫描结果。 |
| [06_Camera_Preview](examples/06_Camera_Preview/) | 在 ST7796 显示屏上预览 OV5647 摄像头。 |
| [07_Camera_ISP_Tuning](examples/07_Camera_ISP_Tuning/) | OV5647 图像传感器 ISP 调参控制。 |
| [08_SD_Card](examples/08_SD_Card/) | MicroSD 卡访问。 |
| [09_Audio_Playback](examples/09_Audio_Playback/) | ES8311 音频播放。 |
| [10_Mic_Record](examples/10_Mic_Record/) | ES8311 麦克风采集。 |

## 显示、摄像头与触摸边界

本产品板载显示屏为 320 × 480 ST7796，通过 80 MHz SPI 连接，并非 MIPI DSI 屏。OV5647
通过 MIPI-CSI 连接。不能将 MIPI DSI PHY 时钟选择应用到这两条路径。如果为外接 DSI 面板
改造 sketch，应使用该面板已经验证的时序，并按[芯片版本说明](../../docs/revisions_ZH.md)为
rev1.x 选择 `PLL_F20M`、为 rev3.x 选择 `XTAL`。

完整 LCD-5 基线保留参考 `displays/` 库及其中的 HX8394 MIPI DSI 配置。LCD-3.5 sketch
不会使用该面板配置；其有效板级层是 `Waveshare_LCD35`，用于驱动板载 ST7796 SPI 屏。
这些参考源码是为保持完整基线而保留，不是 LCD-3.5 的硬件默认配置。

触摸辅助库依次探测 GT911 的 I2C `0x5D`、`0x14`，以响应的地址初始化驱动。它不指定
`INT` 和 `RST`，并使用轮询读取触摸数据。已发布的 LCD-3.5 板使用 FT6336/FT5x06 兼容
触摸控制器，因此仅在两个 GT911 地址均未响应时才回退到 I2C `0x38`。在把任何触摸结果
视为有效前，应在目标板上确认实际控制器。

原理图没有板载 CAN 或 RS-485 收发器。应用可按自身设计外接 CAN 或 RS-485 PHY，但这些
sketch 不声明板载支持这两种接口。
