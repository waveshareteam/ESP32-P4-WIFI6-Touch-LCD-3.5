# ESP32-P4 Silicon Revision Guide

[简体中文](revisions_ZH.md)

This guide uses **silicon revision** throughout. `rev1_3`, `rev3_x`, `prev3`,
and `postv3` describe the ESP32-P4 chip revision; they do not identify a PCB,
display-module, or product hardware revision. Determine the chip revision from
the target device before selecting a non-default profile.

## Supported profiles

| Confirmed ESP32-P4 silicon | ESP-IDF profile | Arduino ChipVariant | Default? |
| --- | --- | --- | --- |
| rev1.x, including rev1.3 (`[1.0, 2.0)`) | `rev1_3` | `prev3` | No |
| rev3.x (`[3.0, 4.0)`) | `rev3_x` | `postv3` | Yes |

New ESP-IDF example configurations default to `rev3_x`. The corresponding
Arduino default is `ChipVariant=postv3`. Use `rev1_3` or `prev3` only for a
confirmed rev1.x chip; their binaries must not be flashed to a rev3.x chip, or
vice versa. See the [CI guide](ci.md#esp32-p4-revision-profiles) for the
ESP-IDF profile commands and artifact safeguards.

## MIPI DSI clock selection

MIPI DSI PHY clock selection must match the ESP32-P4 silicon revision:

| Silicon | DSI PHY clock source | Meaning |
| --- | --- | --- |
| rev1.x / rev1.3 | legacy `PLL_F20M` | Use the legacy DSI PLL-reference source. |
| rev3.x | new `XTAL` | Use the post-v3 DSI PLL-reference source. |

Do not hard-code the rev1.x legacy `PLL_F20M` source for rev3.x, and do not
use the rev3.x `XTAL` source for rev1.x. Arduino DSI panel code must choose the
source from the selected `ChipVariant`; ESP-IDF DSI code must use the matching
revision profile. This rule applies only to a DSI display path.

The supplied LCD on this product is an ST7796 panel connected over SPI at
80 MHz, not a MIPI DSI panel. The OV5647 camera uses MIPI-CSI, which is also a
different path from MIPI DSI. Neither the ST7796 SPI clock nor the OV5647 CSI
clock should be changed as a substitute for the DSI PHY setting. Any external
DSI panel uses its own validated timing and must still follow the silicon clock
rule above.

## Touch-controller probing

The Arduino board library intentionally does not drive GT911 `INT` or `RST`.
GT911 address selection depends on those signals during reset, so it probes
I2C `0x5D` first and then `0x14`; after a probe succeeds it initializes the
driver using that detected address. Touch data is read by polling, not by a
touch interrupt.

The released LCD-3.5 board uses an FT6336/FT5x06-compatible controller. To
keep released boards usable while allowing the GT911 variant, the library falls
back to I2C `0x38` only after both GT911 addresses do not acknowledge. The
fallback is polling as well. A successful probe or compilation is not a
hardware-in-the-loop result; verify the selected controller and touch behavior
on the intended board.

## External interfaces

The schematic does not show an onboard CAN or RS-485 transceiver. An application
may connect a compatible external PHY where its own electrical design requires
one, but this repository does not claim onboard CAN or RS-485 support. The same
hardware-validation boundary applies to all external display and touch variants.
