# Hello world 与系统信息

[English](README.md)

本示例打印 `Hello world!`、检测到的目标芯片与核心数、芯片版本、Flash 大小/类型和
最小空闲堆，然后倒计时十秒并重启芯片。

```text
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

它不需要产品外设、网络凭据或存储设备。pytest 冒烟测试会检查问候语；从 ESP-IDF 示例
继承的其他测试路径还支持 Linux host 与 ESP32 QEMU。
