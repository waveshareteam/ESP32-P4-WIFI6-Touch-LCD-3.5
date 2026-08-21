/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "driver/ppa.h"
#include "app_video.h"
#include "bsp/display.h"

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
#define LCD_BUFFER_COUNT 2
#define CAMERA_BUFFER_COUNT 2
#define DMA_BUFFER_LINES_DEFAULT 120
#define DMA_BUFFER_LINES_MIN 10

static void camera_video_frame_operation(
    uint8_t *camera_buf,
    uint8_t camera_buf_index,
    uint32_t camera_buf_hes,
    uint32_t camera_buf_ves,
    size_t camera_buf_len,
    void *user_data);

static const char *TAG = "app_main";

static ppa_client_handle_t ppa_srm_handle = NULL;
static size_t data_cache_line_size = 0;
static void *lcd_buffer[LCD_BUFFER_COUNT];
static void *lcd_dma_buffer;
static uint32_t lcd_dma_buffer_lines;
static lv_display_t *disp;

static i2c_master_bus_handle_t i2c_bus_;

static bool dummy_draw_enabled = true;
static bool dummy_mode_delay_flag = false;

/* Copy RGB565 pixels while swapping byte order for SPI panel transfer */
static inline void copy_swap_u16_endian(uint16_t *dst, const uint16_t *src, size_t pixel_count)
{
    while (pixel_count > 0) {
        uint16_t p = *src;
        *dst = (uint16_t)((p << 8) | (p >> 8));
        src++;
        dst++;
        pixel_count--;
    }
}

/* Compute a center crop area that matches destination aspect ratio */
static void calc_ppa_input_crop(uint32_t src_w, uint32_t src_h,
                                uint32_t dst_w, uint32_t dst_h,
                                uint32_t *offset_x, uint32_t *offset_y,
                                uint32_t *crop_w, uint32_t *crop_h)
{
    uint64_t src_ratio_l = (uint64_t)src_w * dst_h;
    uint64_t src_ratio_r = (uint64_t)src_h * dst_w;

    if (src_ratio_l > src_ratio_r) {
        *crop_h = src_h;
        *crop_w = (uint32_t)(((uint64_t)src_h * dst_w) / dst_h);
        *offset_x = (src_w > *crop_w) ? (src_w - *crop_w) / 2 : 0;
        *offset_y = 0;
    } else {
        *crop_w = src_w;
        *crop_h = (uint32_t)(((uint64_t)src_w * dst_h) / dst_w);
        *offset_x = 0;
        *offset_y = (src_h > *crop_h) ? (src_h - *crop_h) / 2 : 0;
    }

    if (*crop_w > src_w) {
        *crop_w = src_w;
    }
    if (*crop_h > src_h) {
        *crop_h = src_h;
    }

    *crop_w &= ~1U;
    *crop_h &= ~1U;
    *offset_x &= ~1U;
    *offset_y &= ~1U;

    if (*crop_w == 0) {
        *crop_w = src_w;
    }
    if (*crop_h == 0) {
        *crop_h = src_h;
    }
}

static void camera_video_frame_operation(
    uint8_t *camera_buf,
    uint8_t camera_buf_index,
    uint32_t camera_buf_hes,
    uint32_t camera_buf_ves,
    size_t camera_buf_len,
    void *user_data)
{
    (void)camera_buf_len;
    (void)user_data;
    if (!dummy_draw_enabled || dummy_mode_delay_flag)
    {
        return;
    }

    const uint32_t display_width = BSP_LCD_H_RES;
    const uint32_t display_height = BSP_LCD_V_RES;

    if (display_height == 0)
    {
        ESP_LOGE(TAG, "Display height is zero!");
        return;
    }

    uint32_t in_offset_x = 0;
    uint32_t in_offset_y = 0;
    uint32_t in_block_w = camera_buf_hes;
    uint32_t in_block_h = camera_buf_ves;
    calc_ppa_input_crop(camera_buf_hes, camera_buf_ves,
                        display_width, display_height,
                        &in_offset_x, &in_offset_y,
                        &in_block_w, &in_block_h);

    float scale_x = (float)display_width / (float)in_block_w;
    float scale_y = (float)display_height / (float)in_block_h;

    const uint32_t bytes_per_pixel = (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3);
    const uint32_t buf_index = camera_buf_index % LCD_BUFFER_COUNT;

    ppa_srm_oper_config_t srm_config = {
        .in.buffer = camera_buf,
        .in.pic_w = camera_buf_hes,
        .in.pic_h = camera_buf_ves,
        .in.block_w = in_block_w,
        .in.block_h = in_block_h,
        .in.block_offset_x = in_offset_x,
        .in.block_offset_y = in_offset_y,
        .in.srm_cm = APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,

        .out.buffer = lcd_buffer[buf_index],
        .out.buffer_size = ALIGN_UP(display_width * display_height *
                                        (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3),
                                    data_cache_line_size),
        .out.pic_w = display_width,
        .out.pic_h = display_height,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,

        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = scale_x,
        .scale_y = scale_y,
        .mirror_x = 1,
        .mirror_y = 0,
        .rgb_swap = 0,
        .byte_swap = 0,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_srm_handle, &srm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "PPA SRM failed: %d", ret);
        return;
    }

    /* Push the PPA output to LCD in DMA-friendly stripes */
    const size_t row_bytes = display_width * bytes_per_pixel;
    const uint8_t *src_fb = (const uint8_t *)lcd_buffer[buf_index];
    uint32_t row_start = 0;
    while (row_start < display_height)
    {
        uint32_t block_height = display_height - row_start;
        if (block_height > lcd_dma_buffer_lines)
        {
            block_height = lcd_dma_buffer_lines;
        }

        uint8_t *dst = (uint8_t *)lcd_dma_buffer;
        const uint8_t *src = src_fb + (size_t)row_start * row_bytes;
        if (bytes_per_pixel == 2) {
            copy_swap_u16_endian((uint16_t *)dst, (const uint16_t *)src, (size_t)display_width * block_height);
        } else {
            memcpy(dst, src, (size_t)block_height * row_bytes);
        }

        bool is_last_block = (row_start + block_height) >= display_height;
        ret = esp_lv_adapter_dummy_draw_blit(disp, 0, row_start, display_width, row_start + block_height, dst, is_last_block);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Dummy draw blit failed: %d", ret);
            break;
        }
        row_start += block_height;
    }
}

void app_main(void)
{
    disp = bsp_display_start();
    bsp_display_backlight_on();

    ESP_ERROR_CHECK(esp_lv_adapter_set_dummy_draw(disp, dummy_draw_enabled));

    ppa_client_config_t ppa_srm_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));

    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    i2c_bus_ = bsp_i2c_get_handle();

    esp_err_t ret = app_video_main(i2c_bus_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "video main init failed with err=0x%x", ret);
        return;
    }

    int video_cam_fd = app_video_open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, APP_VIDEO_FMT);
    if (video_cam_fd < 0)
    {
        ESP_LOGE(TAG, "video cam open failed");
        return;
    }

    size_t lcd_buf_size = ALIGN_UP(BSP_LCD_H_RES * BSP_LCD_V_RES * (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3),
                                   data_cache_line_size);
    for (int i = 0; i < LCD_BUFFER_COUNT; i++)
    {
        lcd_buffer[i] = heap_caps_aligned_calloc(data_cache_line_size, 1, lcd_buf_size, MALLOC_CAP_SPIRAM);
        if (lcd_buffer[i] == NULL)
        {
            ESP_LOGE(TAG, "alloc lcd_buffer[%d] failed", i);
            return;
        }
    }

    const uint32_t bytes_per_pixel = (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3);
    lcd_dma_buffer_lines = DMA_BUFFER_LINES_DEFAULT;
    while (lcd_dma_buffer_lines >= DMA_BUFFER_LINES_MIN)
    {
        size_t dma_buf_size = ALIGN_UP(BSP_LCD_H_RES * lcd_dma_buffer_lines * bytes_per_pixel, data_cache_line_size);
        lcd_dma_buffer = heap_caps_aligned_calloc(data_cache_line_size, 1, dma_buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (lcd_dma_buffer != NULL)
        {
            break;
        }
        lcd_dma_buffer_lines /= 2;
    }
    if (lcd_dma_buffer == NULL)
    {
        ESP_LOGE(TAG, "alloc lcd_dma_buffer failed");
        return;
    }

    ESP_LOGI(TAG, "Using user defined buffer");
    void *camera_buf[CAMERA_BUFFER_COUNT];
    for (int i = 0; i < CAMERA_BUFFER_COUNT; i++)
    {
        camera_buf[i] = heap_caps_aligned_calloc(
            data_cache_line_size,
            1,
            app_video_get_buf_size(),
            MALLOC_CAP_SPIRAM);
    }
    ESP_ERROR_CHECK(app_video_set_bufs(video_cam_fd, CAMERA_BUFFER_COUNT, (void *)camera_buf));
    ESP_ERROR_CHECK(app_video_register_frame_operation_cb(camera_video_frame_operation));
    ESP_ERROR_CHECK(app_video_stream_task_start(video_cam_fd, 0, NULL));

}
