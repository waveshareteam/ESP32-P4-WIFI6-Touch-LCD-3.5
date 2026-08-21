# 在产品 LCD 上显示摄像头画面

[English](README.md)

| 支持目标 | 开发板 |
| --- | --- |
| ESP32-P4 | Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 |

本示例通过 `esp_video` V4L2 设备采集板载 MIPI-CSI 摄像头帧，使用 ESP32-P4 PPA
裁剪和缩放，并把 RGB565 条带绘制到板载 320 × 480 LCD。它不面向
ESP32-P4-Function-EV-Board，也不需要外接 7 英寸显示屏。

## 要求

- ESP-IDF 5.4 或更高版本；仓库 CI 当前检查 v5.5.5 和 v6.0.2。
- 已连接 OV5647 MIPI-CSI 摄像头的 ESP32-P4-WIFI6-Touch-LCD-3.5。
- 连接开发板 USB-UART 口的 USB 线，用于烧录和日志。

默认配置选择 OV5647 RAW10、1280 × 960、45 fps。摄像头是否可用、实际帧率和画质
仍取决于所连接模块，必须通过实机验证。

## 数据路径与内存

应用在 PSRAM 中分配两个摄像头 USERPTR 缓冲区和两个 LCD 帧缓冲区，并使用一块支持
DMA 的内部内存条带完成 PPA 到 LCD 的复制。帧回调按 320 × 480 比例居中裁剪，执行
水平镜像，缩放为 RGB565，为显示路径交换字节后提交给 BSP 显示适配层。

V4L2 辅助代码接受两个或三个帧缓冲区。`VIDIOC_STREAMON`、任务创建、缓冲区索引或
注册回调无效时，流启动会安全失败。

## 配置、烧录与监视

在本示例目录执行：

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

只有更换已连接的摄像头或格式时才需要修改摄像头传感器菜单。不要把 SC2336 或
Function-EV 的接线说明用于本产品。按 `Ctrl-]` 退出串口监视。

Actions 编译版本以及“编译证据”和“板级验证”的边界详见仓库
[CI 策略](../../../docs/ci_ZH.md)。
