# ESP32-P4 Eye 摄像与视觉示例

[English](README.md)

本产品应用组合了 OV5647 MIPI-CSI 摄像头、320 × 480 UI、拍照/录像、间隔拍摄、
相册、USB 大容量存储以及端侧行人或人脸检测。默认页面是主菜单，默认 AI 模式为行人检测。

UI 中包含摄像、间隔拍摄、录像、AI 检测、相册、USB 磁盘和设置入口。出现菜单入口并不
代表对应外设已经就绪；启动日志会分别报告初始化失败。

## 运行要求

- 带可用 MIPI-CSI 摄像设备的 ESP32-P4-WIFI6-Touch-LCD-3.5。
- MicroSD 卡，用于保存照片/视频、相册索引和 USB MSC 导出。
- 当前 BSP 组合使用 GPIO 35 单按键路径：单击为确认、双击向下、三击向上、长按菜单。
- ESP-IDF 5.3 或更高版本；仓库 CI 检查 v5.5.5 与 v6.0.2，并按版本处理 TinyUSB、
  `esp_video`、`esp_h264` 和 ESP-DL 兼容逻辑。

应用会在运行时监视存储卡。挂载成功后创建 `/sdcard/esp32_p4_pic_save`、初始化相册并
安装 USB MSC；拔卡后卸载相关服务。使用 USB 大容量存储或拍摄功能前请备份存储卡。

应用初始化行人和人脸检测路径。虽然工程中存在 COCO 模型组件与 UI 资源，但经审计的
运行时选择器没有启用 COCO 检测模式。QMA6100 源文件也被排除在构建之外，因此当前配置
不能描述为已启用 IMU 功能。

## 配置、烧录与监视

在本示例目录执行：

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

默认 MIPI-CSI Kconfig 使用 SCCB I2C port 0，摄像头 SCL 为 GPIO 34、SDA 为 GPIO 31。
修改这些值前请确认摄像头模块和硬件版本。

IDF 6 兼容层还会在配置阶段检查预期的 TinyUSB 与 ESP-DL 源码布局。详情见
[组件策略](../../../docs/components_ZH.md)和 [CI 策略](../../../docs/ci_ZH.md)。Actions 编译
证据不能替代摄像头、SD、USB、按键、显示或 AI 运行时验证。
