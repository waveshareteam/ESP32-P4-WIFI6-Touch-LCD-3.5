# Interactive I2C tools

[简体中文](README_ZH.md)

This example starts an ESP-IDF console REPL and registers `i2cconfig`,
`i2cdetect`, `i2cget`, `i2cset`, and `i2cdump`. On this product, the default I2C
bus is port 0 with SCL on GPIO 8 and SDA on GPIO 7.

Internal pull-ups are disabled by default; use the product's existing I2C bus or
provide suitable external pull-ups when attaching another device. The default
configuration stores command history in a FAT partition at `/data/history.txt`.

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

Run `help` in the monitor before writing registers. Confirm the device voltage,
address, register map, and whether another product component already owns the
bus. An incorrect `i2cset` can change peripheral state until reset or power-off.
