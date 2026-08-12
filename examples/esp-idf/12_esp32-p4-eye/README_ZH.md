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

## ESP32-P4 芯片版本配置

v3 之前的芯片使用 `rev1_3`，其中
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` 且
`CONFIG_ESP32P4_REV_MIN_100=y`。v3 或之后的芯片使用 `rev3_x`，其中
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n` 且
`CONFIG_ESP32P4_REV_MIN_300=y`。两种配置使用独立 sdkconfig 和构建目录，生成的二进制
互不兼容。

该维护产品源码在 ESP-IDF v6.0.2 上为两种配置分别生成带配置标识的任务和制品，并绑定
PR 分支的最终 HEAD，而不是临时合并提交。仅面向 ESP32-P4 主机的包不包含 ESP32-C6
固件，禁止显式整片或区域擦除操作，并拒绝越过 32 MiB 制品策略上限的烧录范围；正常
`write_flash` 仍可能擦除它实际写入的扇区。打包器和烧录器会分别校验 ESP 镜像头为
ESP32-P4 芯片 ID 18，同时
允许原始分区表、NVS 和数据项，并检查偏移、SHA-256 哈希和文件大小。烧录器会探测并
再次探测芯片版本：版本低于 3 时仅接受 `rev1_3`，版本为 3 或更高时仅接受 `rev3_x`。
芯片版本不能确定 PCB 版本或电气版本。

托管 BSP 使用 `SDMMC_SLOT_NO_CD`，因此本应用的遗留存在状态辅助函数会始终把存储卡视为
已插入。启动时会尝试挂载，成功后创建 `/sdcard/esp32_p4_pic_save`、初始化相册并安装
USB MSC。程序不能检测物理拔卡，也不会因此自动卸载；请先备份存储卡，并在存储、USB
大容量存储或拍摄功能可能活动时先关闭开发板电源，再取出存储卡。

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
证据不是硬件在环，不能替代摄像头、SD、USB、按键、显示或 AI 运行时验证。
