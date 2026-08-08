# 最小工程骨架

[English](README.md)

这是仓库中最小的工程布局，只包含根 `CMakeLists.txt`、一个 `main` 组件和空的
`app_main()`。它用于作为新 ESP-IDF 应用的干净起点，不是硬件功能演示。

```text
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

未修改的应用不会输出串口日志，也不会初始化开发板外设。随着工程扩展，请显式添加依赖
和 BSP 初始化。仓库 CI 使用 ESP-IDF v5.5.5 与 v6.0.2 检查此骨架。
