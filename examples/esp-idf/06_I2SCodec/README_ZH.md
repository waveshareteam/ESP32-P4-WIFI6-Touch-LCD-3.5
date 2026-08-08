# ES8311 I2S Codec 示例

[English](README.md)

本示例通过 I2C 和全双工 I2S 驱动板载 ES8311。产品默认 I2C SDA/SCL 为 GPIO 7/8；
I2S MCLK 13、BCLK 12、LRCLK 10、扬声器数据 9、麦克风数据 11、功放使能 53。

音频格式为 16 kHz、16-bit 双声道，MCLK 倍频为 384。`Example Configuration`
提供两种模式：

- `music`（默认）循环播放内嵌的 `canon.pcm`；
- `echo` 读取板载麦克风并写到输出，可配置麦克风增益和播放音量。

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

特别是在 echo 模式下，请从较低音量开始，避免声学反馈。编译通过不能验证实机麦克风
极性、扬声器输出、噪声或增益。
