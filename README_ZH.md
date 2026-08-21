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
    <a href="#-固件">📦 固件</a> ·
    <a href="examples/esp-idf/">🧩 ESP-IDF 示例</a> ·
    <a href="examples/arduino/">🔧 Arduino 示例</a> ·
    <a href="schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf">🧾 原理图</a>
  </p>
  <img src="assets/ESP32-P4-WIFI6-Touch-LCD-3.5-details-1.jpg" alt="微雪 ESP32-P4-WIFI6-Touch-LCD-3.5 开发板正反面产品图" width="600">
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
3. 打开 [`examples/esp-idf/`](examples/esp-idf/) 下的工程，并查看其配置文件。
4. 将目标芯片设置为 `esp32p4`，然后使用 ESP-IDF 配置、构建、烧录并监视所选工程。

新建示例配置默认使用 `rev3_x` ESP32-P4 芯片 profile。仅当芯片探测或其他可信信息确认
为 rev1.x 芯片（包括 rev1.3）时才使用 `rev1_3`。这些名称表示芯片 revision，不是 PCB 或
产品硬件 revision；两种 profile 的二进制互不兼容。明确的 profile 选择命令和配置差异
见 [CI 说明](docs/ci_ZH.md#esp32-p4-芯片版本配置)。

修改芯片变体或 MIPI DSI 时钟设置前，请先阅读[芯片版本说明](docs/revisions_ZH.md)。这些
设置对应芯片 revision，而非 PCB 版本标签。

Wi-Fi 示例通过 ESP32-C6 协处理器通信。调整相关依赖时，请保持主机端组件与
协处理器固件兼容。

## 🧪 ESP-IDF 示例

| 示例 | 功能 |
| --- | --- |
| [01_HowToCreateProject](examples/esp-idf/01_HowToCreateProject/) | 最小 ESP-IDF 工程结构 |
| [02_HelloWorld](examples/esp-idf/02_HelloWorld/) | 基础应用和系统信息 |
| [03_i2c_tools](examples/esp-idf/03_i2c_tools/) | I2C 总线工具和设备发现 |
| [04_wifistation](examples/esp-idf/04_wifistation/) | 通过 ESP32-C6 协处理器连接 Wi-Fi |
| [05_sdmmc](examples/esp-idf/05_sdmmc/) | MicroSD 卡访问 |
| [06_I2SCodec](examples/esp-idf/06_I2SCodec/) | ES8311 音频输入和输出 |
| [07_Displaycolorbar](examples/esp-idf/07_Displaycolorbar/) | LCD 初始化和彩条显示 |
| [08_lvgl_demo_v9](examples/esp-idf/08_lvgl_demo_v9/) | LVGL 9 显示和触摸示例 |
| [09_video_lcd_display](examples/esp-idf/09_video_lcd_display/) | 在 LCD 上显示 OV5647 摄像头视频 |
| [10_mp4_player](examples/esp-idf/10_mp4_player/) | MP4 视频与音频播放 |
| [11_esp_brookesia_phone](examples/esp-idf/11_esp_brookesia_phone/) | ESP-Brookesia 手机风格界面 |
| [12_esp32-p4-eye](examples/esp-idf/12_esp32-p4-eye/) | 摄像、相册和端侧视觉示例 |

## 🧪 Arduino 示例

[`examples/arduino/`](examples/arduino/) 提供 10 个第一方 sketch：Hello World、ASCII 表、
绘图板、LVGL 9、Wi-Fi 分析器、摄像头预览、摄像头 ISP 调参、SD 卡、音频播放和麦克风录音。
默认 rev3.x 芯片配置应安装 Arduino-ESP32 3.3.11 并选择 `ChipVariant=postv3`；仅当芯片
确认属于 rev1.x（含 rev1.3）时选择 `ChipVariant=prev3`。随仓库提供的板级库依赖
GFX Library for Arduino 1.6.7；LVGL 示例还使用 LVGL 9.3.0。板卡选项、库安装、触摸探测和
每个 sketch 的范围见 [Arduino 说明](examples/arduino/README_ZH.md)。

| Sketch | 功能 |
| --- | --- |
| [01_HelloWorld](examples/arduino/examples/01_HelloWorld/) | ST7796 彩条与文字显示 |
| [02_AsciiTable](examples/arduino/examples/02_AsciiTable/) | ST7796 ASCII 字符表 |
| [03_Drawing_board](examples/arduino/examples/03_Drawing_board/) | 显示绘图与触摸 |
| [04_LVGLV9_Arduino](examples/arduino/examples/04_LVGLV9_Arduino/) | LVGL 9 显示与触摸 |
| [05_GFX_ESPWiFiAnalyzer](examples/arduino/examples/05_GFX_ESPWiFiAnalyzer/) | Wi-Fi 扫描显示 |
| [06_Camera_Preview](examples/arduino/examples/06_Camera_Preview/) | OV5647 预览 |
| [07_Camera_ISP_Tuning](examples/arduino/examples/07_Camera_ISP_Tuning/) | OV5647 ISP 控制 |
| [08_SD_Card](examples/arduino/examples/08_SD_Card/) | MicroSD 卡访问 |
| [09_Audio_Playback](examples/arduino/examples/09_Audio_Playback/) | 音频播放 |
| [10_Mic_Record](examples/arduino/examples/10_Mic_Record/) | 麦克风采集 |

## ✅ 持续集成

[ESP-IDF 示例工作流](.github/workflows/esp-idf.yml)会动态发现
`examples/esp-idf/` 下的所有直接子工程，并对完整 Git diff 分类。单工程源码修改只构建
受影响工程；共享文件、工作流或未知构建输入会选择全部工程。文档和治理修改仍会报告
稳定的 CI 门禁，但跳过固件矩阵。标准矩阵仍为 12 个工程 × ESP-IDF v5.5.5/v6.0.2 =
24 次构建，且全部使用面向 rev3.x 芯片的 `rev3_x` 配置，不会为每个芯片版本配置翻倍。维护的产品
源码 `12_esp32-p4-eye` 还会在 IDF v6.0.2 上生成带配置标识的 `rev1_3` 与 `rev3_x`
产品任务/制品；两种配置使用独立 sdkconfig/构建目录，二进制互不兼容。确切配置和烧录
保护见 [CI 说明](docs/ci_ZH.md)。

Arduino CI 会独立发现并使用默认 `postv3` 配置编译全部 10 个 Arduino sketch，使用
Arduino-ESP32 3.3.11。构建成功仍仅代表编译证据，不证明显示、触摸、摄像头、音频、SD 卡或
外接接口能在实机上正常工作。

每个成功的矩阵任务都会根据该工程的 `flasher_args.json` 上传可刷写制品，仓库
`firmware/` 下已有的预编译镜像不会混入 CI 产物。项目选择、版本更新、制品内容和
必需检查行为和验证边界详见 [CI 说明](docs/ci_ZH.md)。

## 📦 固件 <a id="-固件"></a>

[`firmware/ESP32-P4-WiFi6-LCD-3in5.bin`](firmware/ESP32-P4-WiFi6-LCD-3in5.bin)
是仓库中保存的预编译固件镜像，并非 ESP-IDF 源码工程或 CI 构建产物。
请按照官方产品文档提供的方式进行烧录。

该预编译镜像的源码和构建说明目前尚未包含在本仓库中，后续更新可能会补充。源码 CI
不会重新封装或上传此文件；任何替换都必须另行提供发布来源和硬件验证证据。

## 🗂️ 仓库结构

| 路径 | 用途 |
| --- | --- |
| [`examples/esp-idf/`](examples/esp-idf/) | 第一方 ESP-IDF 工程 |
| [`examples/arduino/`](examples/arduino/) | 第一方 Arduino sketch 与板级库 |
| [`firmware/`](firmware/) | 预编译固件镜像 |
| [`schematic/`](schematic/) | 产品原理图 |
| [`assets/`](assets/) | 文档使用的产品图片 |
| [`docs/`](docs/) | CI、组件、芯片版本与维护策略 |
| [`.github/workflows/`](.github/workflows/) | 示例、产品固件、仓库策略与文档 CI |
| [`scripts/`](scripts/) | CI 发现、制品打包与文档验证辅助脚本 |

## 📚 文档与支持

- [产品页面](https://www.waveshare.net/shop/ESP32-P4-WIFI6-Touch-LCD-3.5.htm)
- [产品文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-3.5/)
- [原理图](schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf)
- [CI 与制品策略](docs/ci_ZH.md)
- [组件维护策略](docs/components_ZH.md)
- [ESP32-P4 芯片版本说明](docs/revisions_ZH.md)
- [贡献指南](CONTRIBUTING_ZH.md)
- [支持指南](SUPPORT_ZH.md)
- [提交 Issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5/issues/new)

提交问题时，请提供示例路径、ESP-IDF 版本、复现步骤、预期行为、实际行为以及
相关串口日志。联系 Waveshare 获取产品支持时，也请提供订单号。

Actions 通过只代表编译与打包证据，并非板级功能证明。显示、触摸、摄像头、音频、存储、
USB、电源以及 ESP32-C6 hosted Wi-Fi 仍必须在目标硬件上验证；本仓库不作硬件验证声明。

## 📄 许可证

本仓库基于 Apache License 2.0 许可。详情请参阅 [LICENSE](LICENSE)。

仓库内附带的第三方组件仍分别遵循其各自的许可证条款。
