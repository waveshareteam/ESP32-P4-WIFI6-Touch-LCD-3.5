#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <demos/lv_demos.h>
#include "serial_log.h"
#include "lcd35_board.h"
#include "lcd35_touch.h"

namespace {

constexpr uint32_t kLvglTickPeriodMs = 5;
constexpr uint16_t kDrawBufferHeight = 50;
Arduino_GFX *gfx = lcd35::display();
lcd35::Touch touch;
bool touch_available = false;
static lv_display_t *lv_display;
static lv_indev_t *indev_touchpad;
static lv_color_t *lv_draw_buf1;
static lv_color_t *lv_draw_buf2;

}  // namespace

static void haltWithError(const char *message) {
  serial_log::println(message);
  while (true) {
    delay(1000);
  }
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (!touch_available) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  lcd35::TouchPoint point;
  if (touch.read(&point, 1) != 0) {
    data->point.x = constrain(point.x, 0, lcd35::kLcdWidth - 1);
    data->point.y = constrain(point.y, 0, lcd35::kLcdHeight - 1);
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void lvglTick(void *param) {
  lv_tick_inc(kLvglTickPeriodMs);
}

void setup(void) {
  serial_log::begin(115200);
  serial_log::println("LCD-3.5 LVGL v9 widgets demo");

  if (!lcd35::display_begin()) {
    haltWithError("ST7796 display initialization failed");
  }

  touch_available = touch.begin();
  if (touch_available) {
    serial_log::printf("%s touch found at 0x%02X; polling enabled\n",
                       touch.controller_name(), touch.address());
  } else {
    serial_log::println("No supported touch controller found");
  }

  lv_init();

  const size_t draw_buf_pixels = lcd35::kLcdWidth * kDrawBufferHeight;
  const size_t draw_buf_bytes = draw_buf_pixels * sizeof(lv_color_t);
  lv_draw_buf1 = (lv_color_t *)heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_SPIRAM);
  lv_draw_buf2 = (lv_color_t *)heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_SPIRAM);
  if (!lv_draw_buf1 || !lv_draw_buf2) {
    if (lv_draw_buf1) {
      heap_caps_free(lv_draw_buf1);
      lv_draw_buf1 = NULL;
    }
    if (lv_draw_buf2) {
      heap_caps_free(lv_draw_buf2);
      lv_draw_buf2 = NULL;
    }
    haltWithError("LVGL draw buffer allocation failed!");
  }

  lv_display = lv_display_create(lcd35::kLcdWidth, lcd35::kLcdHeight);
  if (!lv_display) {
    haltWithError("LVGL display allocation failed!");
  }
  lv_display_set_flush_cb(lv_display, my_disp_flush);
  lv_display_set_buffers(lv_display, lv_draw_buf1, lv_draw_buf2, draw_buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  indev_touchpad = lv_indev_create();
  lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev_touchpad, my_touchpad_read);

  const esp_timer_create_args_t lvgl_timer_args = {
    .callback = &lvglTick,
    .name = "lvgl_timer"
  };
  esp_timer_handle_t lvgl_timer;
  esp_timer_create(&lvgl_timer_args, &lvgl_timer);
  esp_timer_start_periodic(lvgl_timer, kLvglTickPeriodMs * 1000);

  lv_display_set_dpi(lv_display, 150);
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);

  lv_demo_widgets();

  serial_log::println("Setup complete");
}

void loop() {
  lv_timer_handler();
  delay(5);
}
