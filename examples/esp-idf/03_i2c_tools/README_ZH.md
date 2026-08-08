# 交互式 I2C 工具

[English](README.md)

本示例启动 ESP-IDF console REPL，并注册 `i2cconfig`、`i2cdetect`、`i2cget`、
`i2cset` 和 `i2cdump`。在本产品上，默认使用 I2C port 0，SCL 为 GPIO 8，SDA 为
GPIO 7。

默认关闭内部上拉；连接其他设备时，请使用产品已有 I2C 总线或提供合适的外部上拉。
默认配置会把命令历史保存在 FAT 分区 `/data/history.txt`。

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

写寄存器前先在监视器中运行 `help`，并确认设备电压、地址、寄存器表以及是否已有产品
组件占用总线。错误的 `i2cset` 可能改变外设状态，直到复位或断电。
