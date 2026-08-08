# MicroSD 读写测试

[English](README.md)

本示例通过 4 位 SDMMC 把产品 MicroSD 卡槽挂载到 `/sdcard`，打印卡信息，并执行
创建、读取、重命名和删除测试。ESP32-P4 默认引脚与产品原理图一致：D0–D3 为
GPIO 39–42、CMD 为 GPIO 44、CLK 为 GPIO 43，内部 SDMMC LDO 使用通道 4。

> [!WARNING]
> 默认配置会在挂载失败时格式化，并且示例会写入、重命名和删除测试文件。请先备份，
> 不要使用包含重要数据的存储卡。

默认总线频率上限为 26 MHz。另一个 `FORMAT_SD_CARD` 选项会显式格式化存储卡，破坏性
更强。

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

运行测试前请插卡。Actions 构建通过不能验证存储卡信号质量、文件系统兼容性或持续 I/O。
