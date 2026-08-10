# 组件维护策略

[English](components.md)

本仓库同时使用托管组件和项目内组件。项目内组件并不等同于技术债务：部分目录承载
开发板行为或示例专用集成，在没有行为变化的前提下无法直接替换。

## 当前分类

| 组件 | 位置 | 结论 |
| --- | --- | --- |
| ESP32-P4-WIFI6-Touch-LCD-3.5 BSP 变体 | 示例 07–12 | 保留为项目内组件 |
| `bsp_extra` | 示例 08、09、12 | 保留为示例专用板级胶水 |
| `sd_card` | 示例 05 | 保留为示例测试支持 |
| `esp_painter` | 示例 12 | 保留为产品 UI/绘制支持 |
| `esp_extractor` | 示例 10、11 | 与其目标相关预编译库一起保留 |
| 检测模型封装 | 示例 12 | 保留在本地，通过组件管理器使用 `espressif/esp-dl ==3.1.3` |
| `espressif/button` | 示例 12 | 托管组件，固定为 `==4.2.0` |
| `espressif/esp_lvgl_port` | 示例 12 | 托管组件，为 ESP-IDF 5.5/6.0 CI 矩阵固定为 `==2.8.0~1` |

ESP-IDF 组件注册表已经发布
[`waveshare/esp32_p4_wifi6_touch_lcd_3_5` v2.0.0](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_3_5/versions/2.0.0)。
它是重要的上游参考，但不能直接替换本仓库中的六个变体。

## BSP 变体为何继续保留在本地

本地 BSP 与注册表版本的板级 API 和产品 GPIO 大体相同，但经审计并非行为等价：

- 本地源文件名和组件名与注册表包不同；
- 至少一条显示路径的颜色顺序不同（本地 RGB、上游 BGR），直接替换会导致红蓝互换；
- 本地 Kconfig 符号和默认值并不相同；
- 示例 07–11 使用 `esp_lvgl_adapter` 和 LVGL `>=8,<10` 集成，而示例 12 使用
  `esp_lvgl_port` 与 LVGL `8.3.*`；
- 每个工程目前都保留其应用实际使用的组件依赖图。

因此，只有在迁移能够证明 API、Kconfig、颜色顺序、显示时序、触摸、音频、SD、USB
和 LVGL 等价后，才可合并这些组件。迁移后必须让两个固定 ESP-IDF 矩阵版本全部通过，
并继续执行板级硬件测试。

## 原理图核对后的不变量

原理图与本地 BSP 头文件对关键 GPIO 的定义一致：

| 功能 | GPIO |
| --- | --- |
| I2C | SCL 8、SDA 7 |
| I2S / ES8311 | SCLK 12、MCLK 13、LRCLK 10、DOUT 9、DSIN 11、功放使能 53 |
| LCD 控制 | MOSI 20、时钟 21、CS 23、D/C 26、复位 27、背光 28 |
| 触摸 | 复位 29、中断 50 |
| SDMMC | D0–D3 39–42、CMD 44、CLK 43 |

这些引脚、显示颜色顺序和依赖组合都属于敏感评审项，组件升级不得静默改写。

## 依赖更新规则

1. 普通托管库优先使用有边界的兼容范围；涉及跨版本 API 或预编译 ABI 时使用精确版本。
2. 保留 TinyUSB、`esp_video` 和 `esp_h264` 针对 ESP-IDF 5.5/6.0 的明确条件；
   不同示例使用了不同功能代际，不能只因为存在新版本就强行统一。
3. 每次只更新一个依赖族，并在 manifest 中记录原因。
4. 示例 12 的 `espressif/esp_lvgl_port` 保持 `==2.8.0~1`，直到 CI 矩阵跨过
   ESP-IDF 6.0 或完成对应的 DPI callback API 迁移；`2.9.0` 依赖后续的 callback API。
5. 以提交后的 GitHub Actions 矩阵作为编译证据。构建通过不能替代显示、触摸、摄像头、
   音频、存储、USB 或 hosted Wi-Fi 的实机验证。
