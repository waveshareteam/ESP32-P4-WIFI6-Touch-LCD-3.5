/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>

#ifndef MAP_FAILED
// ESP-IDF 5.5 newlib headers do not expose the POSIX mmap failure sentinel.
#define MAP_FAILED ((void *)-1)
#endif

#include "esp_err.h"
#include "esp_log.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "app_video.h"

static const char *TAG = "app_video";

#define MAX_BUFFER_COUNT                (6)
#define MIN_BUFFER_COUNT                (2)
#define VIDEO_TASK_STACK_SIZE           (4 * 1024)
#define VIDEO_TASK_PRIORITY             (6)

typedef struct {
    uint8_t *camera_buffer[MAX_BUFFER_COUNT];
    size_t camera_buffer_length[MAX_BUFFER_COUNT];
    size_t camera_buf_size;
    uint8_t camera_buf_count;
    uint32_t camera_buf_hes;
    uint32_t camera_buf_ves;
    struct v4l2_buffer v4l2_buf;
    uint8_t camera_mem_mode;
    app_video_frame_operation_cb_t user_camera_video_frame_operation_cb;
    TaskHandle_t video_stream_task_handle;
    int video_fd;
    SemaphoreHandle_t video_stop_sem;
    SemaphoreHandle_t video_state_mutex;
} app_video_t;

static app_video_t app_camera_video = {
    .video_fd = -1,
};

static esp_err_t app_video_lock_device(int video_fd)
{
    if (video_fd < 0 || app_camera_video.video_state_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (app_camera_video.video_fd != video_fd) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static void app_video_unlock_device(void)
{
    xSemaphoreGive(app_camera_video.video_state_mutex);
}

esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle)
{
    const esp_video_init_csi_config_t base_csi_config = {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
            },
            .freq      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
    };

    esp_video_init_csi_config_t csi_config = base_csi_config;
    if (i2c_bus_handle != NULL) {
        csi_config.sccb_config.init_sccb = false;
        csi_config.sccb_config.i2c_handle = i2c_bus_handle;
    }

    esp_video_init_config_t cam_config = {
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR > 0
        .csi      = &csi_config,
#endif
    };

    return esp_video_init(&cam_config);
}

int app_video_open(char *dev, video_fmt_t init_fmt)
{
    struct v4l2_format default_format;
    struct v4l2_capability capability;
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Open video failed");
        return -1;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        goto exit_0;
    }

    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);

    memset(&default_format, 0, sizeof(struct v4l2_format));
    default_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &default_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        goto exit_0;
    }

    ESP_LOGI(TAG, "width=%" PRIu32 " height=%" PRIu32, default_format.fmt.pix.width, default_format.fmt.pix.height);

    if (default_format.fmt.pix.pixelformat != init_fmt) {
        struct v4l2_format format = {
            .type = type,
            .fmt.pix.width = default_format.fmt.pix.width,
            .fmt.pix.height = default_format.fmt.pix.height,
            .fmt.pix.pixelformat = init_fmt,
        };

        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            ESP_LOGE(TAG, "failed to set format");
            goto exit_0;
        }
    }

    if (app_camera_video.video_stop_sem == NULL && app_camera_video.video_state_mutex == NULL) {
        app_camera_video.video_stop_sem = xSemaphoreCreateBinary();
        if (app_camera_video.video_stop_sem == NULL) {
            ESP_LOGE(TAG, "failed to create video stop semaphore");
            close(fd);
            return -1;
        }
        app_camera_video.video_state_mutex = xSemaphoreCreateMutex();
        if (app_camera_video.video_state_mutex == NULL) {
            ESP_LOGE(TAG, "failed to create video state mutex");
            vSemaphoreDelete(app_camera_video.video_stop_sem);
            app_camera_video.video_stop_sem = NULL;
            close(fd);
            return -1;
        }
        xSemaphoreGive(app_camera_video.video_stop_sem);
    } else if (app_camera_video.video_stop_sem == NULL || app_camera_video.video_state_mutex == NULL) {
        ESP_LOGE(TAG, "video lifecycle state is inconsistent");
        close(fd);
        return -1;
    }

    if (xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        close(fd);
        return -1;
    }
    if (app_camera_video.video_fd >= 0) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        ESP_LOGE(TAG, "video device is already open");
        close(fd);
        return -1;
    }
    app_camera_video.camera_buf_hes = default_format.fmt.pix.width;
    app_camera_video.camera_buf_ves = default_format.fmt.pix.height;
    app_camera_video.video_fd = fd;
    xSemaphoreGive(app_camera_video.video_state_mutex);

    return fd;
exit_0:
    close(fd);
    return -1;
}

esp_err_t app_video_set_bufs(int video_fd, uint32_t fb_num, const void **fb)
{
    esp_err_t lock_result = app_video_lock_device(video_fd);
    if (lock_result != ESP_OK) {
        return lock_result;
    }
    if (app_camera_video.video_stream_task_handle != NULL) {
        ESP_LOGE(TAG, "cannot replace frame buffers while the stream task is running");
        app_video_unlock_device();
        return ESP_ERR_INVALID_STATE;
    }
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num is too large");
        app_video_unlock_device();
        return ESP_FAIL;
    } else if (fb_num < MIN_BUFFER_COUNT) {
        ESP_LOGE(TAG, "At least two buffers are required");
        app_video_unlock_device();
        return ESP_FAIL;
    }

    const void *user_buffers[MAX_BUFFER_COUNT] = {};
    if (fb != NULL) {
        for (uint32_t i = 0; i < fb_num; i++) {
            if (fb[i] == NULL) {
                ESP_LOGE(TAG, "frame buffer is NULL");
                app_video_unlock_device();
                return ESP_ERR_INVALID_ARG;
            }
            user_buffers[i] = fb[i];
        }
    }

    if (app_camera_video.camera_mem_mode == V4L2_MEMORY_MMAP) {
        for (uint32_t i = 0; i < app_camera_video.camera_buf_count; i++) {
            if (app_camera_video.camera_buffer[i] != NULL && app_camera_video.camera_buffer_length[i] != 0) {
                munmap(app_camera_video.camera_buffer[i], app_camera_video.camera_buffer_length[i]);
            }
        }
    }
    memset(app_camera_video.camera_buffer, 0, sizeof(app_camera_video.camera_buffer));
    memset(app_camera_video.camera_buffer_length, 0, sizeof(app_camera_video.camera_buffer_length));
    app_camera_video.camera_buf_count = 0;
    app_camera_video.camera_buf_size = 0;

    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    memset(&req, 0, sizeof(req));
    req.count = fb_num;
    req.type = type;

    app_camera_video.camera_mem_mode = req.memory = fb ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;

    if (ioctl(video_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "req bufs failed");
        goto errout_req_bufs;
    }
    if (req.count != fb_num) {
        ESP_LOGE(TAG, "video driver provided %" PRIu32 " buffers, expected %" PRIu32, req.count, fb_num);
        goto errout_req_bufs;
    }
    for (uint32_t i = 0; i < fb_num; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = req.memory;
        buf.index = i;

        if (ioctl(video_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "query buf failed");
            goto errout_req_bufs;
        }

        if (req.memory == V4L2_MEMORY_MMAP) {
            app_camera_video.camera_buffer[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd, buf.m.offset);
            if (app_camera_video.camera_buffer[i] == MAP_FAILED) {
                app_camera_video.camera_buffer[i] = NULL;
                ESP_LOGE(TAG, "mmap failed");
                goto errout_req_bufs;
            }
            app_camera_video.camera_buffer_length[i] = buf.length;
        } else {
            buf.m.userptr = (unsigned long)user_buffers[i];
            app_camera_video.camera_buffer[i] = (uint8_t *)user_buffers[i];
            app_camera_video.camera_buffer_length[i] = buf.length;
        }

        app_camera_video.camera_buf_size = buf.length;

        if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "queue frame buffer failed");
            goto errout_req_bufs;
        }
    }

    app_camera_video.camera_buf_count = fb_num;
    app_video_unlock_device();
    return ESP_OK;

errout_req_bufs:
    if (req.memory == V4L2_MEMORY_MMAP) {
        for (uint32_t i = 0; i < fb_num; i++) {
            if (app_camera_video.camera_buffer[i] != NULL && app_camera_video.camera_buffer_length[i] != 0) {
                munmap(app_camera_video.camera_buffer[i], app_camera_video.camera_buffer_length[i]);
            }
        }
    }
    req.count = 0;
    if (ioctl(video_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGW(TAG, "failed to release video buffers after setup error");
    }
    memset(app_camera_video.camera_buffer, 0, sizeof(app_camera_video.camera_buffer));
    memset(app_camera_video.camera_buffer_length, 0, sizeof(app_camera_video.camera_buffer_length));
    app_camera_video.camera_buf_count = 0;
    app_camera_video.camera_buf_size = 0;
    app_video_unlock_device();
    return ESP_FAIL;
}

esp_err_t app_video_get_bufs(int fb_num, void **fb)
{
    if (fb == NULL || app_camera_video.video_state_mutex == NULL ||
        xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (app_camera_video.video_fd < 0) {
        app_video_unlock_device();
        return ESP_ERR_INVALID_STATE;
    }
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num is too large");
        app_video_unlock_device();
        return ESP_FAIL;
    } else if (fb_num < MIN_BUFFER_COUNT) {
        ESP_LOGE(TAG, "At least two buffers are required");
        app_video_unlock_device();
        return ESP_FAIL;
    }

    for (int i = 0; i < fb_num; i++) {
        if (app_camera_video.camera_buffer[i] != NULL) {
            fb[i] = app_camera_video.camera_buffer[i];
        } else {
            ESP_LOGE(TAG, "frame buffer is NULL");
            app_video_unlock_device();
            return ESP_FAIL;
        }
    }

    app_video_unlock_device();
    return ESP_OK;
}

uint32_t app_video_get_buf_size(void)
{
    if (app_camera_video.video_state_mutex == NULL ||
        xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    uint32_t buf_size = app_camera_video.camera_buf_hes * app_camera_video.camera_buf_ves * (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3);

    app_video_unlock_device();
    return buf_size;
}

static inline esp_err_t video_receive_video_frame(int video_fd)
{
    memset(&app_camera_video.v4l2_buf, 0, sizeof(app_camera_video.v4l2_buf));
    app_camera_video.v4l2_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    app_camera_video.v4l2_buf.memory = app_camera_video.camera_mem_mode;

    int res = ioctl(video_fd, VIDIOC_DQBUF, &(app_camera_video.v4l2_buf));
    if (res != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static inline esp_err_t video_operation_video_frame(void)
{
    uint8_t buf_index = app_camera_video.v4l2_buf.index;
    if (buf_index >= app_camera_video.camera_buf_count ||
        app_camera_video.camera_buffer[buf_index] == NULL) {
        ESP_LOGE(TAG, "invalid dequeued frame buffer index: %u", buf_index);
        return ESP_FAIL;
    }
    if (app_camera_video.user_camera_video_frame_operation_cb == NULL) {
        ESP_LOGE(TAG, "video frame callback is not registered");
        return ESP_FAIL;
    }
    if (app_camera_video.camera_mem_mode == V4L2_MEMORY_USERPTR) {
        app_camera_video.v4l2_buf.m.userptr =
            (unsigned long)app_camera_video.camera_buffer[buf_index];
        app_camera_video.v4l2_buf.length = app_camera_video.camera_buf_size;
    }

    app_camera_video.user_camera_video_frame_operation_cb(
                        app_camera_video.camera_buffer[buf_index],
                        buf_index,
                        app_camera_video.camera_buf_hes,
                        app_camera_video.camera_buf_ves,
                        app_camera_video.camera_buf_size
                    );
    return ESP_OK;
}

static inline esp_err_t video_free_video_frame(int video_fd)
{
    if (ioctl(video_fd, VIDIOC_QBUF, &(app_camera_video.v4l2_buf)) != 0) {
        ESP_LOGE(TAG, "failed to free video frame");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static inline esp_err_t video_stream_start(int video_fd)
{
    ESP_LOGI(TAG, "Video Stream Start");

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type)) {
        ESP_LOGE(TAG, "failed to start stream");
        goto errout;
    }

    struct v4l2_format format = {0};
    format.type = type;
    if (ioctl(video_fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "get fmt failed");
        if (ioctl(video_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "failed to roll back stream after format query failure");
        }
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static inline esp_err_t video_stream_stop(int video_fd)
{
    ESP_LOGI(TAG, "Video Stream Stop");

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMOFF, &type)) {
        ESP_LOGE(TAG, "failed to stop stream");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static void video_stream_task(void *arg)
{
    int video_fd = *((int *)arg);

    while (ulTaskNotifyTake(pdTRUE, 0) == 0) {
        if (video_receive_video_frame(video_fd) != ESP_OK) {
            break;
        }
        if (ulTaskNotifyTake(pdTRUE, 0) != 0) {
            video_free_video_frame(video_fd);
            break;
        }
        if (video_operation_video_frame() != ESP_OK) {
            video_free_video_frame(video_fd);
            break;
        }
        if (video_free_video_frame(video_fd) != ESP_OK) {
            break;
        }
    }

    if (video_stream_stop(video_fd) != ESP_OK) {
        ESP_LOGE(TAG, "failed to stop video stream task cleanly");
    }
    xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY);
    app_camera_video.video_stream_task_handle = NULL;
    xSemaphoreGive(app_camera_video.video_state_mutex);
    xSemaphoreGive(app_camera_video.video_stop_sem);
    vTaskDelete(NULL);
}

esp_err_t app_video_stream_task_start(int video_fd, int core_id)
{
    if (video_fd < 0) {
        ESP_LOGE(TAG, "video fd is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (app_camera_video.video_stop_sem == NULL ||
        xSemaphoreTake(app_camera_video.video_stop_sem, 0) != pdTRUE) {
        ESP_LOGE(TAG, "video stream task is running or stopping");
        return ESP_ERR_INVALID_STATE;
    }
    if (app_camera_video.video_state_mutex == NULL ||
        xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        xSemaphoreGive(app_camera_video.video_stop_sem);
        return ESP_ERR_INVALID_STATE;
    }
    if (app_camera_video.video_stream_task_handle != NULL) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        xSemaphoreGive(app_camera_video.video_stop_sem);
        ESP_LOGE(TAG, "video stream task state is inconsistent");
        return ESP_ERR_INVALID_STATE;
    }
    if (app_camera_video.video_fd != video_fd) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        xSemaphoreGive(app_camera_video.video_stop_sem);
        ESP_LOGE(TAG, "video fd does not match the open device");
        return ESP_ERR_INVALID_STATE;
    }
    if (app_camera_video.user_camera_video_frame_operation_cb == NULL) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        xSemaphoreGive(app_camera_video.video_stop_sem);
        ESP_LOGE(TAG, "video frame callback is not registered");
        return ESP_ERR_INVALID_STATE;
    }

    if (video_stream_start(video_fd) != ESP_OK) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        xSemaphoreGive(app_camera_video.video_stop_sem);
        return ESP_FAIL;
    }

    BaseType_t result = xTaskCreatePinnedToCore(video_stream_task, "video stream task", VIDEO_TASK_STACK_SIZE, &app_camera_video.video_fd, VIDEO_TASK_PRIORITY, &app_camera_video.video_stream_task_handle, core_id);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "failed to create video stream task");
        goto errout;
    }

    xSemaphoreGive(app_camera_video.video_state_mutex);
    return ESP_OK;

errout:
    app_camera_video.video_stream_task_handle = NULL;
    video_stream_stop(video_fd);
    xSemaphoreGive(app_camera_video.video_state_mutex);
    xSemaphoreGive(app_camera_video.video_stop_sem);
    return ESP_FAIL;
}

esp_err_t app_video_stream_task_stop(int video_fd)
{
    if (app_camera_video.video_state_mutex == NULL ||
        xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    if (app_camera_video.video_fd != video_fd) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    TaskHandle_t task = app_camera_video.video_stream_task_handle;
    if (task == NULL) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    xTaskNotifyGive(task);
    xSemaphoreGive(app_camera_video.video_state_mutex);

    return ESP_OK;
}

esp_err_t app_video_wait_video_stop(void)
{
    if (app_camera_video.video_stop_sem == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(app_camera_video.video_stop_sem, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    xSemaphoreGive(app_camera_video.video_stop_sem);
    return ESP_OK;
}

esp_err_t app_video_register_frame_operation_cb(app_video_frame_operation_cb_t operation_cb)
{
    if (operation_cb == NULL || app_camera_video.video_state_mutex == NULL ||
        xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (app_camera_video.video_fd < 0 || app_camera_video.video_stream_task_handle != NULL) {
        app_video_unlock_device();
        return ESP_ERR_INVALID_STATE;
    }
    app_camera_video.user_camera_video_frame_operation_cb = operation_cb;

    app_video_unlock_device();
    return ESP_OK;
}

esp_err_t app_video_close(int video_fd)
{
    if (video_fd < 0 || app_camera_video.video_stop_sem == NULL ||
        app_camera_video.video_state_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (app_camera_video.video_fd != video_fd) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    TaskHandle_t task = app_camera_video.video_stream_task_handle;
    if (task != NULL) {
        xTaskNotifyGive(task);
    }
    xSemaphoreGive(app_camera_video.video_state_mutex);
    if (xSemaphoreTake(app_camera_video.video_stop_sem, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(app_camera_video.video_state_mutex, portMAX_DELAY) != pdTRUE) {
        xSemaphoreGive(app_camera_video.video_stop_sem);
        return ESP_FAIL;
    }
    if (app_camera_video.video_fd != video_fd) {
        xSemaphoreGive(app_camera_video.video_state_mutex);
        xSemaphoreGive(app_camera_video.video_stop_sem);
        return ESP_ERR_INVALID_STATE;
    }

    if (app_camera_video.camera_mem_mode == V4L2_MEMORY_MMAP) {
        for (uint32_t i = 0; i < app_camera_video.camera_buf_count; i++) {
            if (app_camera_video.camera_buffer[i] != NULL && app_camera_video.camera_buffer_length[i] != 0) {
                munmap(app_camera_video.camera_buffer[i], app_camera_video.camera_buffer_length[i]);
            }
        }
    }
    int close_result = close(app_camera_video.video_fd);
    memset(app_camera_video.camera_buffer, 0, sizeof(app_camera_video.camera_buffer));
    memset(app_camera_video.camera_buffer_length, 0, sizeof(app_camera_video.camera_buffer_length));
    app_camera_video.camera_buf_size = 0;
    app_camera_video.camera_buf_count = 0;
    app_camera_video.camera_buf_hes = 0;
    app_camera_video.camera_buf_ves = 0;
    memset(&app_camera_video.v4l2_buf, 0, sizeof(app_camera_video.v4l2_buf));
    app_camera_video.camera_mem_mode = 0;
    app_camera_video.user_camera_video_frame_operation_cb = NULL;
    app_camera_video.video_stream_task_handle = NULL;
    app_camera_video.video_fd = -1;
    app_video_unlock_device();
    xSemaphoreGive(app_camera_video.video_stop_sem);
    if (close_result != 0) {
        ESP_LOGE(TAG, "failed to close video device");
        return ESP_FAIL;
    }
    return ESP_OK;
}
