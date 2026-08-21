#include <vector>
#include <string.h>
#include <stdio.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_private/esp_cache_private.h"

#include "app_pedestrian_detect.h"
#include "app_humanface_detect.h"
#include "app_coco_detect.h"

#include "app_camera_pipeline.hpp"

#include "app_drawing_utils.h"

#include "app_ai_detect.h"

#include "esp_painter.h"

#include "ui_extra.h"

static const char *TAG = "app_ai_detect";

#define DETECT_HEIGHT    240
#define DETECT_WIDTH     240
#define AI_BUFFER_COUNT  5            // Number of AI detection buffers

#define ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))

static esp_painter_handle_t painter = NULL;
static pipeline_handle_t feed_pipeline;
static pipeline_handle_t detect_pipeline;
static TaskHandle_t detect_task_handle = NULL;
static PedestrianDetect *ped_detect = NULL;
static HumanFaceDetect *hum_detect = NULL;
static COCODetect *coco_od_detect = NULL;

static std::list<dl::detect::result_t> detect_results;

static inline void get_center_square_crop(int src_w, int src_h, int *crop_x, int *crop_y, int *crop_size)
{
    int size = (src_w < src_h) ? src_w : src_h;
    *crop_size = size;
    *crop_x = (src_w - size) / 2;
    *crop_y = (src_h - size) / 2;
}

static void rgb565_center_crop_resize_nn(const uint16_t *src, int src_w, int src_h, uint16_t *dst, int dst_w, int dst_h)
{
    int crop_x = 0;
    int crop_y = 0;
    int crop_size = 0;
    get_center_square_crop(src_w, src_h, &crop_x, &crop_y, &crop_size);

    for (int y = 0; y < dst_h; y++) {
        int sy = crop_y + (y * crop_size) / dst_h;
        const uint16_t *src_row = src + sy * src_w;
        uint16_t *dst_row = dst + y * dst_w;
        for (int x = 0; x < dst_w; x++) {
            int sx = crop_x + (x * crop_size) / dst_w;
            dst_row[x] = src_row[sx];
        }
    }
}

static inline int map_coord_detect_to_screen(int v, int crop_origin, int crop_size, int detect_size)
{
    return crop_origin + (v * crop_size) / detect_size;
}

static inline uint16_t maybe_swap_rgb565(uint16_t color, bool swap)
{
    if (swap) {
        return ((color & 0xFF) << 8) | ((color >> 8) & 0xFF);
    }
    return color;
}

static inline uint16_t painter_color_to_rgb565(esp_painter_color_t color)
{
    switch (color) {
    case ESP_PAINTER_COLOR_RED:
        return RGB565(255, 0, 0);
    case ESP_PAINTER_COLOR_GREEN:
        return RGB565(0, 255, 0);
    case ESP_PAINTER_COLOR_BLUE:
        return RGB565(0, 0, 255);
    case ESP_PAINTER_COLOR_WHITE:
        return RGB565(255, 255, 255);
    case ESP_PAINTER_COLOR_BLACK:
        return RGB565(0, 0, 0);
    case ESP_PAINTER_COLOR_YELLOW:
    default:
        return RGB565(255, 255, 0);
    }
}

static void draw_char_rotated_90(uint16_t *rgb_buf, int width, int height, int x, int y, const esp_painter_basic_font_t *font,
                                 esp_painter_color_t color, char c, int box_l, int box_t, int box_r, int box_b)
{
    if (!font || !rgb_buf || c < 32 || c > 127) {
        return;
    }

    const int bytes_per_row = ((int)font->width + 7) / 8;
    const uint8_t *char_bitmap = font->bitmap + (c - 32) * font->height * bytes_per_row;
    const uint16_t rgb565_color = maybe_swap_rgb565(painter_color_to_rgb565(color), true);

    for (int dy = 0; dy < (int)font->height; ++dy) {
        for (int dx = 0; dx < (int)font->width; ++dx) {
            uint8_t byte = char_bitmap[dy * bytes_per_row + (dx / 8)];
            if (byte & (0x80 >> (dx % 8))) {
                int dst_x = x + ((int)font->height - 1 - dy);
                int dst_y = y + dx;
                if (dst_x < box_l || dst_x > box_r || dst_y < box_t || dst_y > box_b) {
                    continue;
                }
                if (dst_x < 0 || dst_x >= width || dst_y < 0 || dst_y >= height) {
                    continue;
                }
                rgb_buf[dst_y * width + dst_x] = rgb565_color;
            }
        }
    }
}

static void draw_label_left_bottom_vertical_rotated(uint16_t *rgb_buf, int width, int height, const std::vector<int> &box, const char *label)
{
    if (!label || box.size() < 4) {
        return;
    }

    const esp_painter_basic_font_t *font = &esp_painter_basic_font_20;
    const int margin = 4;
    const int label_len = (int) strlen(label);
    const int char_w_rot = (int)font->height;
    const int char_h_rot = (int)font->width;
    const int box_l = box[0] + margin;
    const int box_t = box[1] + margin;
    const int box_r = box[2] - margin - 1;
    const int box_b = box[3] - margin - 1;

    if (box_r < box_l || box_b < box_t) {
        return;
    }

    if (char_w_rot > (box_r - box_l + 1) || char_h_rot > (box_b - box_t + 1)) {
        return;
    }

    int max_chars = (box_b - box_t + 1) / char_h_rot;
    if (max_chars <= 0) {
        return;
    }

    int draw_chars = (label_len < max_chars) ? label_len : max_chars;
    int x = box_l;
    int y = box_b - draw_chars * char_h_rot + 1;

    for (int i = 0; i < draw_chars; ++i) {
        draw_char_rotated_90(rgb_buf, width, height, x, y + i * char_h_rot, font, ESP_PAINTER_COLOR_YELLOW, label[i], box_l, box_t, box_r, box_b);
    }
}

/**
 * @brief Structure for managing AI detection buffers
 */
typedef struct {
    void *ai_buffers[AI_BUFFER_COUNT];        // Buffers for AI detection
    size_t ai_buffer_size;                    // Size of each AI buffer
    int current_ai_buffer_index;              // Index of the current buffer being used
    bool ai_buffers_initialized;              // Flag to check if buffers are initialized
} ai_detection_buffers_t;

// Static instance of AI detection buffers
static ai_detection_buffers_t ai_buffers = {
    .ai_buffers = {nullptr},              // Initialize all buffer pointers to null
    .ai_buffer_size = 0,                  // Initialize buffer size to 0
    .current_ai_buffer_index = 0,         // Initialize current index to 0
    .ai_buffers_initialized = false       // Initialize flag to false
};

void camera_dectect_task(void);

esp_err_t app_ai_detect_init(void)
{
    ESP_LOGI(TAG, "Initialize the AI detect");
    ped_detect = get_pedestrian_detect();
    assert(ped_detect != NULL);
    
    hum_detect = get_humanface_detect();
    assert(hum_detect != NULL);

    coco_od_detect = get_coco_detect();
    assert(coco_od_detect != NULL);

    // Initialize esp_painter
    esp_painter_config_t painter_config = {
        .canvas = {
            .width = BSP_LCD_H_RES,
            .height = BSP_LCD_V_RES
        },
        .color_format = ESP_PAINTER_COLOR_FORMAT_RGB565,
        .default_font = &esp_painter_basic_font_20,
        .swap_rgb565 = true
        
    };
    ESP_ERROR_CHECK(esp_painter_init(&painter_config, &painter));

    set_screen_dimensions(BSP_LCD_H_RES, BSP_LCD_V_RES);

    camera_pipeline_cfg_t feed_cfg = {
        .elem_num = 5,
        .elements = NULL,
        .align_size = 1,
        .caps = MALLOC_CAP_SPIRAM,
        .buffer_size = DETECT_WIDTH * DETECT_HEIGHT / 8,
    };

    camera_element_pipeline_new(&feed_cfg, &feed_pipeline);

    camera_pipeline_cfg_t detect_cfg = {
        .elem_num = 5,
        .elements = NULL,
        .align_size = 1,
        .caps = MALLOC_CAP_SPIRAM,
        .buffer_size = 20 * sizeof(int),
    };
    camera_element_pipeline_new(&detect_cfg, &detect_pipeline);

    xTaskCreatePinnedToCore((TaskFunction_t)camera_dectect_task, "Camera Detect", 1024 * 8, NULL, 5, &detect_task_handle, 1);

    return ESP_OK;
}

/**
 * @brief Initialize AI detection buffers
 * 
 * @param cache_line_size Size of cache line for alignment
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_ai_detection_init_buffers(size_t cache_line_size)
{
    esp_err_t ret = ESP_OK;
    
    // Calculate buffer size with alignment
    ai_buffers.ai_buffer_size = ALIGN_UP(BSP_LCD_H_RES * BSP_LCD_V_RES * 2, cache_line_size);
    
    // Allocate buffers for AI detection
    for (int i = 0; i < AI_BUFFER_COUNT; i++) {
        ai_buffers.ai_buffers[i] = heap_caps_aligned_calloc(
            cache_line_size, 1,
            ai_buffers.ai_buffer_size,
            MALLOC_CAP_SPIRAM
        );
        
        if (ai_buffers.ai_buffers[i] == NULL) {
            ESP_LOGE(TAG, "Failed to allocate AI detection buffer %d", i);
            
            // Clean up already allocated buffers
            for (int j = 0; j < i; j++) {
                if (ai_buffers.ai_buffers[j]) {
                    heap_caps_free(ai_buffers.ai_buffers[j]);
                    ai_buffers.ai_buffers[j] = NULL;
                }
            }
            
            return ESP_ERR_NO_MEM;
        }
    }
    
    ai_buffers.ai_buffers_initialized = true;
    ai_buffers.current_ai_buffer_index = 0;
    
    ESP_LOGI(TAG, "AI detection buffers initialized successfully");
    return ret;
}

/**
 * @brief Process frame for AI detection
 * 
 * @param detect_buf Buffer containing the frame to detect
 * @param width Frame width
 * @param height Frame height
 * @param ai_detect_mode Current AI detection mode
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_ai_detection_process_frame(uint8_t *detect_buf, uint32_t width, uint32_t height, int ai_detect_mode)
{
    if (!ai_buffers.ai_buffers_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = ESP_OK;
    
    // Get current AI buffer
    void *current_ai_buffer = ai_buffers.ai_buffers[ai_buffers.current_ai_buffer_index];

    rgb565_center_crop_resize_nn(
        reinterpret_cast<const uint16_t *>(detect_buf),
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<uint16_t *>(current_ai_buffer),
        DETECT_WIDTH,
        DETECT_HEIGHT
    );
    
    // Submit buffer for AI detection
    if(ai_detect_mode == AI_DETECT_FACE) {
        ret = app_humanface_ai_detect((uint16_t*)current_ai_buffer, (uint16_t*)detect_buf, width, height);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Human face detection failed: 0x%x", ret);
        }
    } else if(ai_detect_mode == AI_DETECT_PEDESTRIAN) {
        ret = app_pedestrian_ai_detect((uint16_t*)current_ai_buffer, (uint16_t*)detect_buf, width, height);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Pedestrian detection failed: 0x%x", ret);
        }
    }
    
    // Move to next buffer in circular fashion
    ai_buffers.current_ai_buffer_index = (ai_buffers.current_ai_buffer_index + 1) % AI_BUFFER_COUNT;
    
    return ret;
}

/**
 * @brief Free AI detection buffers and resources
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_ai_detection_deinit(void)
{
    if (!ai_buffers.ai_buffers_initialized) {
        return ESP_OK; // Nothing to clean up
    }
    
    // Free all allocated buffers
    for (int i = 0; i < AI_BUFFER_COUNT; i++) {
        if (ai_buffers.ai_buffers[i] != NULL) {
            heap_caps_free(ai_buffers.ai_buffers[i]);
            ai_buffers.ai_buffers[i] = NULL;
        }
    }
    
    ai_buffers.ai_buffers_initialized = false;
    
    ESP_LOGI(TAG, "AI detection buffers freed successfully");
    return ESP_OK;
}

void camera_dectect_task(void)
{
    while (1) {        
        camera_pipeline_buffer_element *p = camera_pipeline_recv_element(feed_pipeline, portMAX_DELAY);
        if (p && ui_extra_get_current_page() == UI_PAGE_AI_DETECT && ui_extra_is_ui_init()) {
            if (ui_extra_get_ai_detect_mode() == AI_DETECT_PEDESTRIAN) {
                detect_results = app_pedestrian_detect((uint16_t *)p->buffer, DETECT_WIDTH, DETECT_HEIGHT);
            } else if (ui_extra_get_ai_detect_mode() == AI_DETECT_FACE) {
                detect_results = app_humanface_detect((uint16_t *)p->buffer, DETECT_WIDTH, DETECT_HEIGHT);
            }

            camera_pipeline_queue_element_index(feed_pipeline, p->index);

            camera_pipeline_buffer_element *element = camera_pipeline_get_queued_element(detect_pipeline);
            if (element) {
                element->detect_results = &detect_results;

                camera_pipeline_done_element(detect_pipeline, element);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t app_coco_od_detect(uint16_t *data, int width, int height)
{
    ESP_LOGI(TAG, "Detecting COCO objects");
    detect_results = app_coco_detect(data, width, height);
    if (detect_results.size() > 0) {
        uint16_t *rgb_buf = data;
        for (const auto& res : detect_results) {
            const auto& box = res.box;
            
            // Get confidence score
            float score = res.score;
            
            // Only process detections with confidence higher than 30%
            if (score <= 0.30f) {
                continue;  // Skip this detection if confidence is too low
            }
            
            // Check if bounding box is valid
            if (box.size() >= 4 && std::any_of(box.begin(), box.end(), [](int v) { return v != 0; })) {
                draw_rectangle_rgb(rgb_buf, width, height,
                                box[0], box[1], box[2], box[3],
                                0, 0, 255, 0, 0, 5, true);

                // Display COCO detection class name with confidence score
                int category = res.category;
                
                const char* class_name = get_coco_class_name(category);
                char label[64];
                snprintf(label, sizeof(label), "%s", class_name);
                
                draw_label_left_bottom_vertical_rotated(rgb_buf, width, height, box, label);
            }
        }
    }
    return ESP_OK;
}

esp_err_t app_humanface_ai_detect(uint16_t *detect_buf, uint16_t *draw_buf, int width, int height)
{
    // Process input frame
    camera_pipeline_buffer_element *input_element = camera_pipeline_get_queued_element(feed_pipeline);
    if (input_element) {
        input_element->buffer = reinterpret_cast<uint16_t*>(detect_buf);
        camera_pipeline_done_element(feed_pipeline, input_element);
    }

    // Get detection results
    camera_pipeline_buffer_element *detect_element = camera_pipeline_recv_element(detect_pipeline, 0);
    if (detect_element && detect_element->detect_results) {
        uint16_t *rgb_buf = draw_buf;

        int crop_x = 0;
        int crop_y = 0;
        int crop_size = 0;
        get_center_square_crop(width, height, &crop_x, &crop_y, &crop_size);
        
        // Add safety check for detect_results pointer
        std::list<dl::detect::result_t> *results = detect_element->detect_results;
        if (results && !results->empty()) {
            for (const auto& res : *results) {
                const auto& box = res.box;
                // Add additional safety checks
                if (box.size() >= 4 && std::any_of(box.begin(), box.end(), [](int v) { return v != 0; })) {
                    int x1 = map_coord_detect_to_screen(box[0], crop_x, crop_size, DETECT_WIDTH);
                    int y1 = map_coord_detect_to_screen(box[1], crop_y, crop_size, DETECT_HEIGHT);
                    int x2 = map_coord_detect_to_screen(box[2], crop_x, crop_size, DETECT_WIDTH);
                    int y2 = map_coord_detect_to_screen(box[3], crop_y, crop_size, DETECT_HEIGHT);

                    draw_rectangle_rgb(rgb_buf, width, height,
                                    x1, y1, x2, y2,
                                    0, 0, 255, 0, 0, 5, false);

                    if(res.keypoint.size() >= 10 && 
                            std::any_of(res.keypoint.begin(), res.keypoint.end(), [](int v) { return v != 0; })) {
                        std::vector<int> mapped_landmarks;
                        mapped_landmarks.reserve(res.keypoint.size());
                        for (size_t i = 0; i + 1 < res.keypoint.size(); i += 2) {
                            int lx = map_coord_detect_to_screen(res.keypoint[i], crop_x, crop_size, DETECT_WIDTH);
                            int ly = map_coord_detect_to_screen(res.keypoint[i + 1], crop_y, crop_size, DETECT_HEIGHT);
                            mapped_landmarks.push_back(lx);
                            mapped_landmarks.push_back(ly);
                        }
                        draw_green_points(rgb_buf, mapped_landmarks, false);
                    }
                }
            }
        }

        camera_pipeline_queue_element_index(detect_pipeline, detect_element->index);
    }

    return ESP_OK;
}

esp_err_t app_pedestrian_ai_detect(uint16_t *detect_buf, uint16_t *draw_buf, int width, int height)
{
    // Process input frame
    camera_pipeline_buffer_element *input_element = camera_pipeline_get_queued_element(feed_pipeline);
    if (input_element) {
        input_element->buffer = reinterpret_cast<uint16_t*>(detect_buf);
        camera_pipeline_done_element(feed_pipeline, input_element);
    }

    // Get detection results
    camera_pipeline_buffer_element *detect_element = camera_pipeline_recv_element(detect_pipeline, 0);
    if (detect_element && detect_element->detect_results) {
        uint16_t *rgb_buf = draw_buf;

        int crop_x = 0;
        int crop_y = 0;
        int crop_size = 0;
        get_center_square_crop(width, height, &crop_x, &crop_y, &crop_size);
        
        // Add safety check for detect_results pointer
        std::list<dl::detect::result_t> *results = detect_element->detect_results;
        if (results && !results->empty()) {
            for (const auto& res : *results) {
                const auto& box = res.box;
                // Add additional safety checks
                if (box.size() >= 4 && std::any_of(box.begin(), box.end(), [](int v) { return v != 0; })) {
                    int x1 = map_coord_detect_to_screen(box[0], crop_x, crop_size, DETECT_WIDTH);
                    int y1 = map_coord_detect_to_screen(box[1], crop_y, crop_size, DETECT_HEIGHT);
                    int x2 = map_coord_detect_to_screen(box[2], crop_x, crop_size, DETECT_WIDTH);
                    int y2 = map_coord_detect_to_screen(box[3], crop_y, crop_size, DETECT_HEIGHT);

                    draw_rectangle_rgb(rgb_buf, width, height,
                                    x1, y1, x2, y2,
                                    0, 0, 255, 0, 0, 5, false);
                }
            }
        }
    
        camera_pipeline_queue_element_index(detect_pipeline, detect_element->index);
    }

    return ESP_OK;
}
