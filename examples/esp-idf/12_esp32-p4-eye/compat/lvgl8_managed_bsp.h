#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#if LVGL_VERSION_MAJOR == 8
typedef lv_disp_t lv_display_t;
typedef lv_disp_rot_t lv_disp_rotation_t;
#elif LVGL_VERSION_MAJOR == 9
/* The managed BSP's native type spellings are available in LVGL 9. */
#else
#error "lvgl8_managed_bsp.h supports LVGL major versions 8 and 9 only"
#endif
