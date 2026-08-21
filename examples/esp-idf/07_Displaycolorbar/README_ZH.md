# LCD 彩条初始化测试

[English](README.md)

本示例初始化项目内产品 BSP、打开背光，并在 320 × 480 LCD 上依次填充八条水平色带：
红、绿、蓝、黄、紫、青、白、黑。

这是仓库中最小的显示路径测试，适合在引入 LVGL 前检查 LCD 复位、SPI 控制、背光、
像素格式和颜色顺序。产品相关引脚为 LCD MOSI 20、时钟 21、CS 23、D/C 26、复位 27、
背光 28。

```text
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

请把实际显示顺序和颜色通道与上方列表对照。CI 构建通过无法发现屏幕未连接、背光异常或
红蓝互换。用注册表组件替换本地 BSP 前请阅读[组件策略](../../../docs/components_ZH.md)。
