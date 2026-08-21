#pragma once

#include <Arduino.h>
#include "driver/i2c_master.h"

namespace lcd35 {

enum class TouchController : uint8_t {
  none,
  gt911,
  ft5x06,
};

struct TouchPoint {
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t strength = 0;
  uint8_t id = 0;
};

class Touch {
 public:
  bool begin();
  size_t read(TouchPoint *points, size_t capacity);
  TouchController controller() const { return controller_; }
  uint8_t address() const { return address_; }
  const char *controller_name() const;

 private:
  bool open_device(uint8_t address);
  size_t read_gt911(TouchPoint *points, size_t capacity);
  size_t read_ft5x06(TouchPoint *points, size_t capacity);
  esp_err_t read_register8(uint8_t reg, uint8_t *data, size_t length);
  esp_err_t read_register16(uint16_t reg, uint8_t *data, size_t length);
  esp_err_t write_register16(uint16_t reg, uint8_t value);

  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t device_ = nullptr;
  TouchController controller_ = TouchController::none;
  uint8_t address_ = 0;
};

}  // namespace lcd35
