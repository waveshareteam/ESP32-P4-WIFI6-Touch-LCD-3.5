# ESP32-C6 hosted Wi-Fi Station

[English](README.md)

ESP32-P4 本身不集成无线电。本示例通过板载 ESP32-C6 协处理器以及
`esp_wifi_remote`、`esp_hosted` 组件连接 Wi-Fi Station。主机端组件与 ESP32-C6
中运行的固件必须使用兼容的协议代际。

在 `Example Configuration` 中配置 SSID、密码、重试次数、认证阈值和 WPA3 SAE：

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

应用会初始化 NVS 和默认网络接口、启动 Station、重试连接，并报告获得的 IP 或最终
失败，但不会回显配置的 SSID 或密码。仓库默认值与 CI 配置只包含占位符；仍不要将生产
凭据提交到仓库或发布到共享日志。

编译通过不能证明 ESP32-C6 固件兼容或无线功能正常。实机测试时请同时记录主机端和
协处理器版本。
