# ESP-Brookesia 出厂固件

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5%2B-E7352C?logo=espressif)](https://github.com/espressif/esp-idf/releases)
[![Target](https://img.shields.io/badge/target-ESP32--P4-1F6FEB)](https://www.espressif.com/en/products/socs/esp32-p4)
[![LVGL](https://img.shields.io/badge/LVGL-9.5.0-2AA9E0?logo=lvgl)](https://lvgl.io/)

[English](README.md) | 简体中文

本目录是 **Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5** 的 ESP-Brookesia 固件源码，
支持 3.5 英寸 320 × 480 SPI 触摸屏、ES8311 音频编解码器、OV5647 摄像头、
microSD 卡和 ESP32-C6 Hosted Wi-Fi。

## 平台配置

| 项目 | 配置 |
| --- | --- |
| 开发板 | 微雪 [ESP32-P4-WIFI6-Touch-LCD-3.5](https://www.waveshare.net/shop/ESP32-P4-WIFI6-Touch-LCD-3.5.htm) |
| 主控 | ESP32-P4NRW32，16 MB Flash，32 MB PSRAM |
| 显示 / 触摸 | 3.5 英寸 320 × 480 竖屏 IPS，ST7796 驱动，SPI 接口；FT6336 电容触摸，I2C 接口 |
| 框架 | ESP-IDF v5.5 及以上（`main/idf_component.yml`） |
| UI | LVGL 9.5.0，本地 ESP-Brookesia core 0.6.0-beta2；`espressif/esp_lvgl_adapter` ^0.7.3~1 |
| 板级支持 | `waveshare/waveshare_boards` ^0.2.0（板型 `esp32_p4_wifi6_touch_lcd_3_5`），基于 `espressif/esp_board_manager` ^0.7.5，由本地 [`components/board`](components/board/) 封装 |
| 无线 | ESP32-C6 经 SDIO 连接，使用 ESP-Hosted（`espressif/esp_hosted` ^1.4.7、`esp_wifi_remote` 0.14.5）和 `esp_wifi_service` ^0.5.3 |
| 音频 | ES8311 编解码器，板载麦克风和扬声器 |
| 摄像头 | OV5647，MIPI-CSI 接口，使用 `espressif/esp_video` ^2.3.0 |
| 存储 | microSD 卡（4 位 SDMMC）和 SPIFFS `storage` 分区 |
| 语音助手 | `espressif/esp_xiaozhi` 0.1.3~1、ESP-SR 2.4.7、xiaozhi-fonts 1.6.0 |
| 多媒体 | `espressif/esp_extractor` 1.0.7、`esp_new_jpeg` 1.0.2 |

接口与引脚分配见[原理图](../../schematic/ESP32-P4-WIFI6-Touch-LCD-3.5-schematic.pdf)和
[产品文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-3.5/)。
ESP32-P4 芯片版本相关选项位于 `sdkconfig.defaults`，修改前请先阅读
[版本说明](../../docs/revisions_ZH.md)。

## 应用

<table>
  <tr>
    <td align="center"><img src="components/draw/assets/img_app_drawpanel.png" width="64" alt="DrawPanel 图标"><br><b>DrawPanel</b></td>
    <td align="center"><img src="components/brookesia_app_squareline_demo/assets/esp_brookesia_app_icon_launcher_squareline_112_112.png" width="64" alt="Squareline 图标"><br><b>Squareline</b></td>
    <td align="center"><img src="components/brookesia_app_calculator/assets/img_app_calculator.png" width="64" alt="Calculator 图标"><br><b>Calculator</b></td>
    <td align="center"><img src="components/SpecAnalyzer/assets/img_app_specanalyzer.png" width="64" alt="SpecAnalyzer 图标"><br><b>SpecAnalyzer</b></td>
    <td align="center"><img src="components/MusicPlayer/assets/img_app_musicplayer.png" width="64" alt="MusicPlayer 图标"><br><b>MusicPlayer</b></td>
  </tr>
  <tr>
    <td align="center"><img src="components/Camera/assets/img_app_camera.png" width="64" alt="Camera 图标"><br><b>Camera</b></td>
    <td align="center"><img src="components/VideoPlayer/assets/img_app_vedioplayer.png" width="64" alt="VideoPlayer 图标"><br><b>VideoPlayer</b></td>
    <td align="center"><img src="components/Settings/assets/img_app_settings.png" width="64" alt="Settings 图标"><br><b>Settings</b></td>
    <td align="center"><img src="components/XiaozhiApp/assets/img_app_xiaozhi.png" width="64" alt="AIChats 图标"><br><b>AIChats</b></td>
    <td></td>
  </tr>
</table>

| 应用 | 功能 | 运行依赖 |
| --- | --- | --- |
| DrawPanel | 固定画布区域绘图，在边缘绘画不会拖动整个应用 | 显示与触摸 |
| Squareline | 从组件仓库安装的 Squareline Studio UI 演示与应用模板 | 显示与触摸 |
| Calculator | 适合触摸操作的 Brookesia 计算器 | 显示与触摸 |
| SpecAnalyzer | 板载麦克风的单声道 FFT 频谱 | ES8311 输入 |
| MusicPlayer | 播放 `/spiffs/music` 中的 MP3，带频谱可视化 | SPIFFS `storage`，ES8311 输出 |
| Camera | OV5647 MIPI CSI 预览，由 PPA 缩放铺满屏幕 | OV5647，`esp_video` |
| VideoPlayer | 全屏播放 SD 卡中的 AVI/MJPEG 视频，支持 PCM 音频；详见[摄像头与多媒体](#摄像头与多媒体) | SD 卡，JPEG/PPA，ES8311 输出 |
| Settings | WLAN 扫描/连接、声音、屏幕亮度、SNTP 时间和设备信息；亮度与音量保存在 NVS | ESP32-C6 Hosted Wi-Fi |
| AIChats | 网络检查、小智激活、WakeNet 唤醒词、VAD、Opus 上下行、服务端文本和 TTS | Wi-Fi，`model`/`storage` 镜像，ES8311 |

## 编译

### 环境要求

使用 ESP-IDF v5.5 及以上版本和 `esp32p4` 工具链，需要能访问 ESP 组件仓库。
烧录时请使用支持数据传输的 USB 线。

以下命令都在本目录下运行。从仓库根目录开始：

```sh
cd firmware/brookesia
idf.py --version
idf.py set-target esp32p4
idf.py bmgr -b esp32_p4_wifi6_touch_lcd_3_5
idf.py build
```

`set-target` 会下载托管组件，包括 `waveshare/waveshare_boards` 中的板级定义。
`bmgr` 为所选开发板生成 `components/gen_bmgr_codes`，该目录已被 Git 忽略；
更换开发板或更新 `waveshare_boards` 后需要重新运行 `bmgr`。

### 首次完整烧录与分区变更

首次安装必须写入 bootloader、分区表、应用、ESP-SR 模型和 SPIFFS 资源：

```sh
idf.py -p PORT flash monitor
```

修改分区偏移或大小、从旧分区布局升级、或排查模型/文件系统镜像残留时，
先擦除 Flash 再完整烧录：

```sh
idf.py -p PORT erase-flash flash monitor
```

分区布局、语音模型和 `spiffs/` 资源都没有变化时，可以只烧录应用：

```sh
idf.py -p PORT app-flash monitor
```

`app-flash` 不会更新 `srmodels.bin`、`storage.bin` 和分区表。修改
`partitions.csv`、WakeNet/模型 Kconfig、xiaozhi-fonts、内置音乐或提示音后，
需要执行完整 `flash`。

### Flash 分区

16 MB 分区布局定义在 `partitions.csv`。bootloader 位于 `0x2000`，
分区表位于 `0x9000`：

| 名称 | 类型 | 偏移 | 大小 | 用途 |
| --- | --- | --- | --- | --- |
| `nvsfactory` | data / nvs | `0xA000` | 200 KiB | 出厂 NVS 数据 |
| `nvs` | data / nvs | `0x3C000` | 840 KiB | Wi-Fi、应用设置和小智状态 |
| `otadata` | data / ota | `0x10E000` | 8 KiB | OTA 数据（预留） |
| `phy_init` | data / phy | `0x110000` | 4 KiB | PHY 校准数据 |
| `model` | data / spiffs | `0x111000` | 956 KiB | ESP-SR 语音模型（`srmodels.bin`） |
| `factory` | app / factory | `0x200000` | 8 MiB | 应用程序 |
| `storage` | data / spiffs | `0xA00000` | 6 MiB | 音乐、小智提示音和字体（`storage.bin`） |

## AIChats / 小智

AIChats 使用 `esp_xiaozhi` 处理协议，本工程负责 UI、激活、音频和生命周期集成。

首次使用：

1. 在 Settings 中配置 WLAN。
2. 打开 AIChats。如果设备尚未绑定账号，会显示并播报激活码。
3. 在[小智控制台](https://xiaozhi.me)添加设备并输入激活码。
4. 页面进入待机后说出唤醒词，默认是“你好小智”。

如果日志出现 `No speech model found` 或 `contains no compatible models`，
请重新编译并完整烧录，以写入分区表和 `srmodels/srmodels.bin`。

本地 UI 语言和首选传输方式可在以下菜单中选择：

```text
idf.py menuconfig
  Xiaozhi Application
    Local UI language
      English
      Simplified Chinese
    Preferred Xiaozhi transport
      WebSocket
      Auto (prefer MQTT+UDP)
      MQTT+UDP
```

`sdkconfig.defaults` 选择 English 和 Auto；传输方式的 Kconfig 默认值是 WebSocket。
语言选项只影响固件自带的文本，不会翻译服务端返回的对话文本、TTS 或激活消息。
本地字符串位于 `components/XiaozhiApp/XiaozhiLocalization.*`。

开发板的 ES8311 上只有一个麦克风。AIChats 和 SpecAnalyzer 都采集单声道；
AIChats 将其转换为 16 kHz 单声道 Opus 上行，播放链路会按需转换服务端音频以适配
编解码器输出。

## 摄像头与多媒体

Camera 使用 OV5647 的 800 × 640 binning 模式。PPA 缩放画面并居中裁剪，铺满
320 × 480 屏幕；缩放系数取整到 PPA 的 1/16 步进，裁剪尺寸由它推算。

MusicPlayer 播放 `/spiffs/music` 中的 MP3。更换曲目后需要重新生成并烧录
`storage.bin`，仅 `app-flash` 不会更新它。

VideoPlayer 只扫描 `/sdcard/avi`，`.avi` 扩展名不区分大小写。SD 卡根目录下的
AVI 以及 MP4、MOV 等其他容器不会被列出。它使用 `esp_extractor` 解封装 AVI，
用硬件 JPEG 解码器解码 MJPEG（硬件无法解码的帧回退到 `esp_new_jpeg`），并播放
PCM 音频，其他音频格式会被跳过。PPA 把每帧裁切缩放到铺满屏幕，再按每段 32 行
通过 SPI 发送给 ST7796。80 MHz 的 SPI 时钟把整帧刷新限制在约 30 fps，落后于音频的
帧会被丢弃。

## 开发说明

- 应用通过 `ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR` 自行注册。
  `main/main.cpp` 中的 `BOOT_APP_GROUPS` 只决定主屏顺序以及各组安装时显示的启动阶段；
  已注册但未列出的应用会在最后一组之后安装。
- 新应用继承 `esp_brookesia::systems::phone::App` 并实现 `run()` 和 `back()`，
  其他生命周期函数按资源归属按需重写。每个组件在自己的 `idf_component.yml`
  中声明依赖，`main` 只保留直接使用的依赖。
- 硬件访问统一经过 [`components/board`](components/board/)（`board_display`、
  `board_audio`、`board_camera`、`board_storage`、`board_wifi`），它基于
  `esp_board_manager`。应用调用这些 `board_*` 接口，不直接操作底层驱动。
- MCP 工具入口是 `components/XiaozhiApp/XiaozhiDeveloperTools.cpp` 中的
  `registerXiaozhiDeveloperTools()`。只注册已实现且返回真实结果的工具，
  耗时操作放在应用任务中执行。
- 在 LVGL 线程中访问 LVGL 对象，或短暂持有显示锁。应用关闭时释放其定时器、
  任务、队列、编解码器会话、SD 句柄和 LVGL 对象。

## 注意事项

- LVGL 锁只用于 UI 操作。网络、SD 扫描、音频初始化、NVS 访问和模型加载都应在锁外执行。
- 修改模型、字体、提示音、音乐或分区后需要完整 `flash`。
- 音频应用共用同一条物理总线，切换应用时要停止或交接编解码器会话。
- Host 组件需要与 ESP32-C6 上的协处理器固件保持兼容。

`main/main.cpp` 中的 `EXAMPLE_SHOW_MEM_INFO` 当前为 `true`，会周期性统计 SRAM/PSRAM；
详细日志输出由 `EXAMPLE_SHOW_MEM_INFO_LOG` 单独控制。

反馈问题时，请提供开发板版本、ESP-IDF 版本、完整复现步骤和日志中第一个实际错误。

本仓库使用 [Apache-2.0 许可证](../../LICENSE)。本地 Brookesia core 另附
[Apache-2.0 许可证文件](components/brookesia_core/license.txt)，仅适用于该组件；
`components/XiaozhiApp/third_party/` 下的第三方代码保留其自身许可证。
