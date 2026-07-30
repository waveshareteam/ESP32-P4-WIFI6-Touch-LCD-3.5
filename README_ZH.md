<div align="center">
  <h1>ESP32-P4-WIFI6-Touch-LCD-3.5</h1>
  <p><strong>ESP32-P4 3.5 英寸 320 × 480 SPI LCD 智能视觉开发板</strong></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/esp-idf.yml"><img alt="ESP-IDF 示例构建" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/esp-idf.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/docs.yml"><img alt="文档检查" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/actions/workflows/docs.yml/badge.svg"></a>
    <a href="LICENSE"><img alt="许可证" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5"></a>
  </p>
  <p>
    <a href="README.md">English</a> ·
    <a href="https://www.waveshare.net/shop/ESP32-P4-WIFI6-Touch-LCD-3.5.htm">🌐 产品页面</a> ·
    <a href="https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-3.5/">📚 产品文档</a> ·
    <a href="example/ESP-IDF/">🧩 ESP-IDF 示例</a> ·
    <a href="schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf">🧾 原理图</a>
  </p>
  <img src="assets/ESP32-P4-WIFI6-Touch-LCD-3.5-details-1.jpg" alt="Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5" width="600">
</div>

---

## ✨ 概述

本仓库提供适用于 Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 的第一方
ESP-IDF 示例、预编译固件镜像和产品原理图。

该开发板将 ESP32-P4 应用处理器、ESP32-C6 无线协处理器、3.5 英寸电容触摸屏、
摄像头、音频、存储和电源管理外设集成于一体，适用于智能显示、语音与视觉交互、
边缘 AI 以及其他交互式嵌入式应用。

## 🖥️ 硬件概览

| 功能 | 器件 / 接口 |
| --- | --- |
| 主处理器 | ESP32-P4NRW32，高性能双核加低功耗单核 RISC-V |
| 存储 | 32 MB 叠封 PSRAM 和 16 MB 外置 NOR Flash |
| 无线连接 | ESP32-C6FH8，通过 SDIO 提供 2.4 GHz Wi-Fi 6 和 Bluetooth 5 LE |
| 显示屏 | 3.5 英寸 320 × 480 IPS LCD，使用 ST7796 控制器和 SPI 接口 |
| 触摸 | FT6336 电容触摸控制器，通过 I2C 通信 |
| 摄像头 | 500 万像素 OV5647，通过 MIPI-CSI 通信 |
| 音频 | ES8311 编解码器、板载麦克风和 8 Ω / 2 W 扬声器 |
| 电源 | AXP2101 电源管理和 3.7 V 锂电池接口 |
| 存储卡 | MicroSD 卡槽，使用 SDIO 3.0 |
| USB | USB-UART Type-C 和 USB 2.0 High-Speed OTG Type-C |
| 硬件文件 | [原理图](schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf) |

> [!NOTE]
> 无线连接由板载 ESP32-C6 协处理器提供；ESP32-P4 本身不集成 Wi-Fi 或
> Bluetooth 无线电。

## 🚀 快速开始

1. 阅读官方[产品文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-3.5/)。
2. 安装所选示例要求的 ESP-IDF 版本。
3. 打开 [`example/ESP-IDF/`](example/ESP-IDF/) 下的工程，并查看其配置文件。
4. 将目标芯片设置为 `esp32p4`，然后使用 ESP-IDF 配置、构建、烧录并监视所选工程。

Wi-Fi 示例通过 ESP32-C6 协处理器通信。调整相关依赖时，请保持主机端组件与
协处理器固件兼容。

## 🧪 ESP-IDF 示例

| 示例 | 功能 |
| --- | --- |
| [01_HowToCreateProject](example/ESP-IDF/01_HowToCreateProject/) | 最小 ESP-IDF 工程结构 |
| [02_HelloWorld](example/ESP-IDF/02_HelloWorld/) | 基础应用和系统信息 |
| [03_i2c_tools](example/ESP-IDF/03_i2c_tools/) | I2C 总线工具和设备发现 |
| [04_wifistation](example/ESP-IDF/04_wifistation/) | 通过 ESP32-C6 协处理器连接 Wi-Fi |
| [05_sdmmc](example/ESP-IDF/05_sdmmc/) | MicroSD 卡访问 |
| [06_I2SCodec](example/ESP-IDF/06_I2SCodec/) | ES8311 音频输入和输出 |
| [07_Displaycolorbar](example/ESP-IDF/07_Displaycolorbar/) | LCD 初始化和彩条显示 |
| [08_lvgl_demo_v9](example/ESP-IDF/08_lvgl_demo_v9/) | LVGL 9 显示和触摸示例 |
| [09_video_lcd_display](example/ESP-IDF/09_video_lcd_display/) | 在 LCD 上显示 OV5647 摄像头视频 |
| [10_mp4_player](example/ESP-IDF/10_mp4_player/) | MP4 视频与音频播放 |
| [11_esp_brookesia_phone](example/ESP-IDF/11_esp_brookesia_phone/) | ESP-Brookesia 手机风格界面 |
| [12_esp32-p4-eye](example/ESP-IDF/12_esp32-p4-eye/) | 摄像、相册和端侧视觉示例 |

本仓库目前不包含 Arduino 示例。

## ✅ 持续集成

[ESP-IDF 示例工作流](.github/workflows/esp-idf.yml)会动态发现
`example/ESP-IDF/` 下的所有直接子工程。遇到影响构建的修改时，每个工程都会针对
`esp32p4` 使用 ESP-IDF v5.5.5 和 v6.0.2 编译。

仅修改 README 时只运行轻量的文档检查，不会启动固件构建矩阵。

每个成功的矩阵任务都会根据该工程的 `flasher_args.json` 上传可刷写制品，仓库
`firmware/` 下已有的预编译镜像不会混入 CI 产物。项目选择、版本更新、制品内容和
验证边界详见 [CI 说明](docs/ci.md)。

## 📦 固件

[`firmware/ESP32-P4-WiFi6-LCD-3in5.bin`](firmware/ESP32-P4-WiFi6-LCD-3in5.bin)
是仓库中保存的预编译固件镜像，并非 ESP-IDF 源码工程或 CI 构建产物。
请按照官方产品文档提供的方式进行烧录。

该预编译镜像的源码和构建说明目前尚未包含在本仓库中，后续更新可能会补充。

## 🗂️ 仓库结构

| 路径 | 用途 |
| --- | --- |
| [`example/ESP-IDF/`](example/ESP-IDF/) | 第一方 ESP-IDF 工程 |
| [`firmware/`](firmware/) | 预编译固件镜像 |
| [`schematic/`](schematic/) | 产品原理图 |
| [`assets/`](assets/) | 文档使用的产品图片 |
| [`docs/`](docs/) | CI 与固件维护说明 |
| [`.github/workflows/`](.github/workflows/) | ESP-IDF 构建与文档检查 |
| [`scripts/`](scripts/) | CI 发现、制品打包与文档验证辅助脚本 |

## 📚 文档与支持

- [产品页面](https://www.waveshare.net/shop/ESP32-P4-WIFI6-Touch-LCD-3.5.htm)
- [产品文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-3.5/)
- [原理图](schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf)
- [提交 Issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/issues/new)

提交问题时，请提供示例路径、ESP-IDF 版本、复现步骤、预期行为、实际行为以及
相关串口日志。联系 Waveshare 获取产品支持时，也请提供订单号。

## 📄 许可证

本仓库基于 Apache License 2.0 许可。详情请参阅 [LICENSE](LICENSE)。

仓库内附带的第三方组件仍分别遵循其各自的许可证条款。
