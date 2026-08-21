#include <Arduino_GFX_Library.h>

#include "lcd35_board.h"
#include "lcd35_touch.h"
#include "serial_log.h"

namespace {

constexpr size_t kMaxTouchPoints = 5;
Arduino_GFX *gfx = lcd35::display();
lcd35::Touch touch;
bool touch_available = false;

}  // namespace

void setup() {
  serial_log::begin(115200);
  serial_log::println("LCD-3.5 drawing board");

  if (!lcd35::display_begin()) {
    serial_log::println("ST7796 display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  touch_available = touch.begin();
  if (touch_available) {
    serial_log::printf("%s touch found at 0x%02X; polling enabled\n",
                       touch.controller_name(), touch.address());
  } else {
    serial_log::println("No supported touch controller found");
  }

  gfx->fillScreen(RGB565_WHITE);
}

void loop() {
  if (!touch_available) {
    delay(100);
    return;
  }

  lcd35::TouchPoint points[kMaxTouchPoints];
  const size_t point_count = touch.read(points, kMaxTouchPoints);
  for (size_t index = 0; index < point_count; ++index) {
    if (points[index].x < gfx->width() && points[index].y < gfx->height()) {
      gfx->fillCircle(points[index].x, points[index].y, 5, RGB565_BLUE);
    }
  }

  if (point_count == 0) {
    delay(10);
  }
}
