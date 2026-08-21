#include <Arduino_GFX_Library.h>

#include "lcd35_board.h"
#include "serial_log.h"

Arduino_GFX *gfx = lcd35::display();

void setup() {
  serial_log::begin(115200);
  serial_log::println("LCD-3.5 Hello World");

  if (!lcd35::display_begin()) {
    serial_log::println("ST7796 display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->fillScreen(RGB565_RED);
  delay(500);
  gfx->fillScreen(RGB565_GREEN);
  delay(500);
  gfx->fillScreen(RGB565_BLUE);
  delay(500);

  gfx->fillScreen(RGB565_BLACK);
  gfx->setCursor(10, 10);
  gfx->setTextColor(RGB565_WHITE);
  gfx->println("Hello World!");
  serial_log::println("Hello World shown on the ST7796 display");
}

void loop() {
  gfx->setCursor(random(gfx->width()), random(gfx->height()));
  gfx->setTextColor(random(0xffff), random(0xffff));
  gfx->setTextSize(random(1, 6), random(1, 6), random(2));
  gfx->println("Hello World!");
  delay(1000);
}
