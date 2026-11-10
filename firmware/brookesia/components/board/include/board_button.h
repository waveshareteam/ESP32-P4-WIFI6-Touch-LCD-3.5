/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
// Call during serialized application initialization. Board owns the button
// for the firmware lifetime; no callback holds an application pointer.
esp_err_t board_user_button_init(void);
uint32_t board_user_button_get_press_count(void);
#ifdef __cplusplus
}
#endif
