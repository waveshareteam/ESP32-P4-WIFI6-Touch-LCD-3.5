# ESP32-P4-WIFI6-Touch-LCD-3.5 project-local BSP

This directory contains the BSP variant used by its parent example. It targets the
Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5 and keeps the example's tested dependency
profile. The implementation exposes the 320 × 480 RGB565 LCD, touch controller,
ES8311 audio codec, and SDMMC wiring used by this repository.

The component is intentionally project-local. Its source name, Kconfig options,
color-order behavior, and LVGL integration are not identical across all examples
or to the separately released Registry component. Do not replace it mechanically.
See [Component policy](../../../../../docs/components.md) before updating or
consolidating the BSP. Pin assignments remain authoritative in
`include/bsp/esp32_p4_wifi6_touch_lcd_35.h` and are cross-checked against the
repository schematic.

Product page: <https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.5.htm>

## 简体中文

此目录是所属示例使用的项目内 BSP 变体，面向 Waveshare
ESP32-P4-WIFI6-Touch-LCD-3.5，并保留该示例已经验证的依赖组合。当前实现提供
本仓库使用的 320 × 480 RGB565 LCD、触摸、ES8311 音频和 SDMMC 接线支持。

该组件有意保留在各示例内部。不同示例之间以及与组件注册表中独立发布的版本相比，
其源文件名、Kconfig、颜色顺序和 LVGL 集成并不完全等价，请勿直接机械替换。
更新或合并 BSP 前请阅读[组件维护策略](../../../../../docs/components_ZH.md)。引脚定义以
`include/bsp/esp32_p4_wifi6_touch_lcd_35.h` 为准，并已与仓库原理图交叉核对。

产品页：<https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.5.htm>
