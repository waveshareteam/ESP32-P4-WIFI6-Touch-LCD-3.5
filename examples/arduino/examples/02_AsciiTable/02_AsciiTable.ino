#include <Arduino_GFX_Library.h>

#include "lcd35_board.h"
#include "serial_log.h"

Arduino_GFX *gfx = lcd35::display();

void setup() {
  serial_log::begin(115200);
  serial_log::println("LCD-3.5 ASCII table");

  if (!lcd35::display_begin()) {
    serial_log::println("ST7796 display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  const int num_cols = (gfx->width() / 12) - 1;
  const int num_rows = gfx->height() / 16;

  gfx->fillScreen(RGB565_BLACK);

  gfx->setTextColor(RGB565_GREEN);
  for (int x = 0; x < num_cols; ++x) {
    gfx->setCursor(16 + x * 12, 4);
    gfx->print(x);
  }
  gfx->setTextColor(RGB565_BLUE);
  for (int y = 0; y < num_rows; ++y) {
    gfx->setCursor(4, 16 + y * 16);
    gfx->print(y);
  }

  char c = 0;
  for (int y = 0; y < num_rows; ++y) {
    for (int x = 0; x < num_cols; ++x) {
      gfx->drawChar(16 + x * 12, 16 + y * 16, c++, RGB565_WHITE, RGB565_BLACK);
    }
  }
}

void loop() {
}
