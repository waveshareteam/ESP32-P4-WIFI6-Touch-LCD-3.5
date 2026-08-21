/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static char *TAG = "jd9365_test";
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t lcd_io;

static void draw_color_bars(void)
{
    static const uint16_t colors[] = {
        0xF800,
        0x07E0,
        0x001F,
        0xFFE0,
        0xF81F,
        0x07FF,
        0xFFFF,
        0x0000,
    };
    uint16_t line_buf[BSP_LCD_H_RES];
    int color_count = sizeof(colors) / sizeof(colors[0]);
    int bar_width = BSP_LCD_H_RES / color_count;

    int y = 0;
    while (y < BSP_LCD_V_RES) {
        int x = 0;
        while (x < BSP_LCD_H_RES) {
            int bar_index = x / bar_width;
            if (bar_index >= color_count) {
                bar_index = color_count - 1;
            }
            line_buf[x] = colors[bar_index];
            x++;
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, BSP_LCD_H_RES, y + 1, line_buf);
        y++;
    }
}

void app_main(void)
{
    /**
     *    __    ___  ___ _____  __  ____
     *    \ \  /   \/ _ \___ / / /_| ___|
     *     \ \/ /\ / (_) ||_ \| '_ \___ \
     * /\_/ / /_// \__, |__) | (_) |__) |
     * \___/___,'    /_/____/ \___/____/
     */
    printf("   __    ___  ___ _____  __  ____\r\n");
    printf("   \\ \\  /   \\/ _ \\___ / / /_| ___|\r\n");
    printf("    \\ \\/ /\\ / (_) ||_ \\| '_ \\___ \\\r\n");
    printf("/\\_/ / /_// \\__, |__) | (_) |__) |\r\n");
    printf("\\___/___,'    /_/____/ \\___/____/\r\n");
    // unity_run_menu();
    ESP_LOGI(TAG, "Initialize LCD device");
    esp_err_t ret = bsp_display_new(NULL, &panel_handle, &lcd_io);
    if (ret == ESP_OK) {
        bsp_display_brightness_init();
        bsp_display_backlight_on();
    } else {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Show color bar pattern drawn by software");
    draw_color_bars();
}
