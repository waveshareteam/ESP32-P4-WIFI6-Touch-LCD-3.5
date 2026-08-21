#include "lcd35_board.h"

namespace lcd35 {
namespace {

Arduino_HWSPI lcd_bus(
    kLcdDataCommand,
    kLcdChipSelect,
    kLcdClock,
    kLcdMosi,
    GFX_NOT_DEFINED,
    &SPI,
    false);

// Rotation 4 mirrors the X axis without swapping the 320 x 480 geometry. This
// matches the board BSP's ST7796 panel configuration.
Arduino_ST7796 lcd_panel(&lcd_bus, kLcdReset, 4, true, kLcdWidth, kLcdHeight);
bool backlight_attached = false;

}  // namespace

Arduino_GFX *display() {
  return &lcd_panel;
}

void display_brightness(uint8_t brightness_percent) {
  if (brightness_percent > 100) {
    brightness_percent = 100;
  }
  if (!backlight_attached) {
    backlight_attached = ledcAttach(kLcdBacklight, 5000, 10);
  }
  if (backlight_attached) {
    const uint32_t duty = (1023U * brightness_percent) / 100U;
    ledcWrite(kLcdBacklight, duty);
  }
}

bool display_begin(uint8_t brightness_percent) {
  display_brightness(0);
  if (!lcd_panel.begin(kLcdPixelClockHz)) {
    return false;
  }
  display_brightness(brightness_percent);
  return true;
}

}  // namespace lcd35
