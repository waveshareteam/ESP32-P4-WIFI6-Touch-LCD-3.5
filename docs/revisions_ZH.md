# ESP32-P4 芯片版本说明

[English](revisions.md)

本文中的 **芯片版本** 均指 ESP32-P4 的 silicon revision。`rev1_3`、`rev3_x`、
`prev3` 和 `postv3` 表示芯片 revision，不表示 PCB、显示模组或产品硬件版本。在选择非默认
配置前，应先从目标设备确认芯片版本。

## 支持的配置

| 已确认的 ESP32-P4 芯片 | ESP-IDF profile | Arduino ChipVariant | 是否默认 |
| --- | --- | --- | --- |
| rev1.x，含 rev1.3（`[1.0, 2.0)`） | `rev1_3` | `prev3` | 否 |
| rev3.x（`[3.0, 4.0)`） | `rev3_x` | `postv3` | 是 |

新建 ESP-IDF 示例配置默认使用 `rev3_x`，对应的 Arduino 默认项为
`ChipVariant=postv3`。只有已确认芯片属于 rev1.x 时才使用 `rev1_3` 或 `prev3`；两种
二进制不能交叉烧录到 rev3.x 或 rev1.x 芯片。ESP-IDF profile 命令和制品保护见
[CI 说明](ci_ZH.md#esp32-p4-芯片版本配置)。

## MIPI DSI 时钟选择

MIPI DSI PHY 时钟源必须与 ESP32-P4 芯片版本匹配：

| 芯片 | DSI PHY 时钟源 | 含义 |
| --- | --- | --- |
| rev1.x / rev1.3 | 旧版 `PLL_F20M` | 使用旧版 DSI PLL 参考时钟源。 |
| rev3.x | 新版 `XTAL` | 使用 post-v3 DSI PLL 参考时钟源。 |

不得在 rev3.x 上硬编码 rev1.x 的旧版 `PLL_F20M`，也不得在 rev1.x 上使用 rev3.x 的
`XTAL`。Arduino DSI panel 代码必须根据所选 `ChipVariant` 选择时钟源；ESP-IDF DSI 代码
必须使用匹配的芯片 profile。此规则只适用于 DSI 显示路径。

本产品板载显示屏是以 80 MHz SPI 连接的 ST7796，而不是 MIPI DSI 屏。OV5647 摄像头使用
MIPI-CSI，也与 MIPI DSI 属于不同通道。不能将 ST7796 SPI 时钟或 OV5647 CSI 时钟当作 DSI
PHY 设置的替代项。若外接 DSI 面板，应使用其已验证的时序，同时遵守上述芯片时钟规则。

## 触摸控制器探测

Arduino 板级库有意不驱动 GT911 的 `INT` 或 `RST`。GT911 在复位时由这些信号共同决定地址，
因此库会依次探测 I2C `0x5D`、`0x14`，探测成功后以检测到的地址初始化驱动。触摸数据采用
轮询读取，不使用触摸中断。

已发布的 LCD-3.5 板使用 FT6336/FT5x06 兼容控制器。为同时支持 GT911 变体且不影响已发布
板卡，只有当两个 GT911 地址均未响应时，库才回退探测 I2C `0x38`。该回退同样使用轮询。
探测成功或编译通过不属于硬件在环（HIL）结论；应在目标板上确认实际控制器及触摸行为。

## 外接接口

原理图未显示板载 CAN 或 RS-485 收发器。应用可以按自身电气设计外接兼容 PHY，但本仓库不
声明板载 CAN 或 RS-485 支持。所有外接显示和触摸变体同样需要独立的硬件验证。
