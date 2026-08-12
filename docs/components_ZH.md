# 组件维护策略

[English](components.md)

本仓库为单一产品使用公开的托管板级支持包，只在应用确有需要时保留产品专用胶水代码。

## 当前分类

| 组件 | 位置 | 结论 |
| --- | --- | --- |
| ESP32-P4-WIFI6-Touch-LCD-3.5 BSP | 产品示例 | 托管依赖 `waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.0` |
| `bsp_extra` | 存在该目录的产品示例 | 仅保留为产品专用胶水 |
| `sd_card` | 示例 05 | 保留为示例测试支持 |
| `esp_painter` | 示例 12 | 保留为产品 UI/绘制支持 |
| `esp_extractor` | 示例 10、11 | 与其目标相关预编译库一起保留 |
| 检测模型封装 | 示例 12 | 保留在本地，通过组件管理器使用 `espressif/esp-dl ==3.1.3` |
| `espressif/button` | 示例 12 | 托管组件，固定为 `==4.2.0` |
| LVGL 运行时 | 示例 12 | 托管 BSP 引入 `esp_lvgl_adapter`；示例 12 因 21 个 SquareLine 生成的图像资源而直接固定 `lvgl/lvgl` 为 `8.3.*` |

六份复制的本地 BSP 组件变体已删除。公开依赖精确使用
[`waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.0`](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_3_5/versions/2.0.0/readme)。
该注册表版本对应官方不可变源码提交 `a7c084c0425ef104f3ecf288f3afd1ff8ef4f97b`；迁移时已
审计这一提交，产品 manifest 和公开依赖说明均以以上注册表包及版本为准。

## 托管 BSP 边界

不得仅为保留本地变体而恢复复制的板级支持包。只有代码可证明是产品专用应用胶水时才保留
`bsp_extra`；通用板级支持仍使用托管注册表依赖。编译证据不能证明硬件行为。
示例 12 遗留的闪光灯、存储卡存在状态和休眠辅助函数仅保留迁移前的空操作或
`SDMMC_SLOT_NO_CD` 兼容语义，因为托管 BSP 没有提供对应的 GPIO 控制 API；这些函数不是
已经实现相关硬件控制的声明。

## 依赖更新规则

1. 普通托管库优先使用有边界的兼容范围；涉及跨版本 API 或预编译 ABI 时使用精确版本。
2. 产品应用需要时保留明确的 ESP-IDF 兼容条件；组件更新不得变成硬件验证声明。
3. 每次只更新一个依赖族，并在 manifest 中记录原因。
4. 示例 12 的 21 个 SquareLine 生成图像资源使用 LVGL 8 描述符契约，因此保持
   `lvgl/lvgl 8.3.*`。托管 BSP 提供 `esp_lvgl_adapter`；不要添加未被应用直接使用的
   `esp_lvgl_port` 依赖。仅在示例 12 的托管 BSP、`bsp_extra` 和 `main` 目标解析托管 BSP
   公共头时，私有强制包含兼容头会先提供托管公共头缺少的 ESP-IDF/基础 `bool`、`uint32_t`
   与 `esp_err_t` 类型，再将 LVGL 8 的 `lv_disp_t` 和 `lv_disp_rot_t` 拼写映射为 BSP 所需
   类型；它不会恢复本地 BSP，也不会传播到其他示例、目标或全局设置。迁移到 LVGL 9 前必须
   重新生成并审计完整 UI。
5. 以提交后的 GitHub Actions 矩阵作为编译证据。构建通过不是硬件在环（HIL）证据，
   不能替代实机验证。
