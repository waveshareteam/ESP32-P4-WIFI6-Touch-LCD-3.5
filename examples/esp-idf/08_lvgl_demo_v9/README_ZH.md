# LVGL 9 Benchmark

[English](README.md)

本示例通过 LVGL 适配层启动产品显示/触摸 BSP、打开背光，并默认运行
`lv_demo_benchmark()`。工程把 `lvgl/lvgl` 固定在 9.4 系列；源码中保留 music 与
widgets demo 调用，但它们不是默认入口。

默认配置面向 ESP32-P4、16 MB Flash、200 MHz PSRAM、FreeRTOS LVGL OS 层、15 ms
刷新周期和两个 LVGL draw unit。

```text
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p PORT flash monitor
```

Benchmark 会有意增加渲染负载。必须在实机上测量帧率、触摸坐标、颜色顺序、内存占用
和温度；仅编译通过不能证明显示性能。修改 BSP 或 LVGL 集成组合前请阅读
[组件策略](../../../docs/components_ZH.md)。
