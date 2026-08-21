# 组件维护策略

[English](components.md)

本仓库为单一产品使用公开的托管板级支持包，只在应用确有需要时保留产品专用胶水代码。

## 当前分类

| 组件 | 位置 | 结论 |
| --- | --- | --- |
| ESP32-P4-WIFI6-Touch-LCD-3.5 BSP | 产品示例 | 托管依赖 `waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.2` |
| `bsp_extra` | 存在该目录的产品示例 | 仅保留为产品专用胶水 |
| `sd_card` | 示例 05 | 保留为示例测试支持 |
| `esp_painter` | 示例 12 | 保留为产品 UI/绘制支持 |
| `esp_extractor` | 示例 10、11 | 与其目标相关预编译库一起保留 |
| 检测模型封装 | 示例 12 | 保留在本地，通过组件管理器使用 `espressif/esp-dl ==3.1.3` |
| `espressif/button` | 示例 12 | 托管组件，固定为 `==4.2.0` |
| LVGL 运行时 | 示例 12 | 托管 BSP 引入 `esp_lvgl_adapter`；示例 12 因 21 个 SquareLine 生成的图像资源而直接固定 `lvgl/lvgl` 为 `8.3.*` |

六份复制的本地 BSP 组件变体已删除。公开依赖精确使用
[`waveshare/esp32_p4_wifi6_touch_lcd_3_5 ==2.0.2`](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_3_5/versions/2.0.2/readme)。
迁移时已审计该 Registry 包；产品 manifest 和公开依赖说明均以以上已发布包及版本为准。

2.0.2 已发布到 Component Registry，所有声明该 BSP 的产品 manifest 均固定使用该版本。
组件或 BSP 改动即使已合并，在对应 Registry 版本公开发布前也不能成为产品依赖。不得用
Git URL、分支、提交、本地路径或未发布版本替换该版本：这些写法不适用于可发布的产品
manifest，并可能被 Component Registry CI 拒绝。

## 托管 BSP 边界

不得仅为保留本地变体而恢复复制的板级支持包。只有代码可证明是产品专用应用胶水时才保留
`bsp_extra`；通用板级支持仍使用托管注册表依赖。编译证据不能证明硬件行为。
示例 12 遗留的闪光灯、存储卡存在状态和休眠辅助函数仅保留迁移前的空操作或
`SDMMC_SLOT_NO_CD` 兼容语义，因为托管 BSP 没有提供对应的 GPIO 控制 API；这些函数不是
已经实现相关硬件控制的声明。

### 显示配置契约

官方 BSP 2.0.2 公共头将显示固定为 RGB565、大端颜色顺序和每像素 16 位。因此，
示例 09 在应用中无条件使用 RGB565 视频格式契约，而不再选择已移除的 BSP Kconfig
颜色格式选项；示例 10 在应用中将 `APP_LCD_BUFFER_COUNT` 固定为 2，而不再使用已移除的
DPI 缓冲区 Kconfig 选项。这些源码契约不构成硬件在环（HIL）验证声明。

[BSP Pull Request #203](https://github.com/waveshareteam/Waveshare-ESP32-components/pull/203)
通过包含 `esp_err_t` 的定义让 `bsp/display.h` 实现自包含。对应 2.0.2 包发布并验证后，已删除
两处仅作为产品侧兼容处理存在的 screen 源文件 include；直接使用 ESP 错误类型的文件仍会
包含 `esp_err.h`。

## 依赖更新规则

1. 普通托管库优先使用有边界的兼容范围；涉及跨版本 API 或预编译 ABI 时使用精确版本。
2. 产品应用需要时保留明确的 ESP-IDF 兼容条件；组件更新不得变成硬件验证声明。
3. 每次只更新一个依赖族，并在 manifest 中记录原因。
4. 示例 12 的 21 个 SquareLine 生成图像资源使用 LVGL 8 描述符契约，因此保持
   `lvgl/lvgl 8.3.*`。托管 BSP 提供 `esp_lvgl_adapter`；不要添加未被应用直接使用的
   `esp_lvgl_port` 依赖。BSP 2.0.2 已直接提供匹配的 LVGL 8 旋转类型，因此删除原有的产品侧
   私有强制包含兼容头。迁移到 LVGL 9 前仍须重新生成并审计完整 UI。
5. 示例 10 的 `espressif/esp_audio_codec` 保持为 `>=2.3.0,<2.6.0`：v2.6+
   要求 ESP32-P4 为 3 或更新的修订版，而 `rev1_3` 仍保留为显式兼容 profile。该依赖约束
   不构成硬件验证声明。
6. 先提交共享组件改动，再提交消费它的 BSP；只有所需包发布到 Component Registry 后才更新
   产品 manifest。以上应保持为可独立评审的 Pull Request。临时本地组件仅可用于本地排查，
   不得提交为 Registry 发布版本的替代品。
7. LCD-5 的完整 Arduino 库基线以源码形式位于 `examples/arduino/`，LCD-3.5 板级层仍是
   实际硬件适配层；这些 Arduino 源码不会修改或覆盖 ESP-IDF 示例使用的托管 BSP 依赖。
8. 以提交后的 GitHub Actions 矩阵作为编译证据。构建通过不是硬件在环（HIL）证据，
   不能替代实机验证。
