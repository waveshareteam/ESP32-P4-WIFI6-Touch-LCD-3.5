# ESP-Brookesia 手机风格界面

[English](README.md)

| 支持目标 | 开发板 |
| --- | --- |
| ESP32-P4 | Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 |

本示例在产品板载 320 × 480 触摸屏上运行仓库内附带的 ESP-Brookesia Phone 框架和
本地 SquareLine 应用。启动时会选择 320 × 480 样式表分支，并注册启动页、时钟、天气、
通话、聊天、闹钟和音乐控制等 UI 页面。

这是一个本地 UI 演示。默认配置关闭 AI 框架、动画播放器、服务和扬声器功能，
`main.cpp` 也没有挂载 SD 卡，或初始化 Wi-Fi、网络凭据和音频 codec。因此，界面中可见
的页面并不代表已经实现云服务、电话、媒体或语音功能。

## 要求

- ESP-IDF 5.3 或更高版本；仓库 CI 当前检查 v5.5.5 和 v6.0.2。
- ESP32-P4-WIFI6-Touch-LCD-3.5。
- 连接 USB-UART 的 USB 线，用于烧录和日志。

所需 ESP-Brookesia 组件和 SquareLine 生成的应用已经包含在本工程内，不要再向示例中
克隆第二份 `esp-brookesia` 仓库。

## 配置、烧录与监视

在本示例目录执行：

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

修改显示分辨率、LVGL 组合或 BSP 时，必须同时调整样式表选择和项目内 BSP。替换本地
BSP 前请阅读[组件策略](../../../docs/components_ZH.md)。按 `Ctrl-]` 退出串口监视。

仓库 [CI 策略](../../../docs/ci_ZH.md)记录了编译使用的框架版本，并说明为什么构建通过
不能等同于触摸/显示运行时测试。
