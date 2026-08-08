# SD 卡视频播放器

[English](README.md)

| 支持目标 | 开发板 |
| --- | --- |
| ESP32-P4 | Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 |

本示例从 MicroSD 卡读取媒体文件，使用 ESP32-P4 硬件 JPEG 解码器处理 JPEG/MJPEG
视频帧，把 RGB565 图像绘制到板载 320 × 480 LCD，并通过板载 ES8311 播放受支持的
音频。不需要 HDMI 转接板或 ESP32-P4-Function-EV-Board。

## 媒体要求

- 默认文件是 `/sdcard/test_video.mp4`。
- 可在 `Product LCD Media Player Configuration` → `Video File Configuration` 中修改文件名；
  除非同时修改 BSP 设置，挂载点仍为 `/sdcard`。
- 内嵌提取器注册了 MP4 与 AVI 容器；视频路径要求 JPEG/MJPEG 帧，并把解码结果转换为
  RGB565。
- ES8311 初始化成功且容器提供受支持音频流时启用声音；如果 codec 初始化失败，视频会
  继续静音播放。

可使用 Espressif 提供的
[`test_video.mp4`](https://dl.espressif.com/AE/esp-dev-kits/test_video.mp4)
作为起点。媒体兼容性和持续播放仍需在实机上检查；能够识别容器并不代表支持其中所有
编解码组合。

## 准备开发板

1. 使用 ESP-IDF FATFS 支持的文件系统格式化 MicroSD 卡。
2. 把所选文件复制到卡根目录；使用默认设置时命名为 `test_video.mp4`。
3. 在开发板上电或复位前插入存储卡。
4. 连接 USB-UART，用于烧录和日志。

BSP 以 4 位高速模式挂载 SDMMC slot 0。默认不在挂载失败时格式化，因此缺卡或文件系统
不可读时只报告错误，不会重格式化存储卡。

## 配置、烧录与监视

在本示例目录执行：

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

文件播放结束后，播放器会循环重新开始。按 `Ctrl-]` 退出串口监视。

Actions 编译的 ESP-IDF 版本与硬件验证边界详见仓库
[CI 策略](../../../docs/ci_ZH.md)。
