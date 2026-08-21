# ES8311 I2S codec demo

[简体中文](README_ZH.md)

This example drives the onboard ES8311 codec over I2C and full-duplex I2S. The
product defaults are I2C SDA/SCL GPIO 7/8; I2S MCLK 13, BCLK 12, LRCLK 10,
speaker data 9, microphone data 11, and amplifier enable 53.

Audio runs at 16 kHz, 16-bit stereo with an MCLK multiple of 384. `Example
Configuration` provides two modes:

- `music` (default) repeatedly plays the embedded `canon.pcm` sample;
- `echo` reads the onboard microphone and writes it to the output, with
  configurable microphone gain and playback volume.

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

Start with a low volume, especially in echo mode, to avoid acoustic feedback.
Compile success does not verify microphone polarity, speaker output, noise, or
gain on the physical board.
