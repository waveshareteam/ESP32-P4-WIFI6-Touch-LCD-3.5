#include "lcd35_touch.h"

#include <algorithm>

#include "esp_err.h"
#include "esp_log.h"
#include "lcd35_board.h"

namespace lcd35 {
namespace {

constexpr char kTag[] = "lcd35-touch";
constexpr uint8_t kGt911PrimaryAddress = 0x5D;
constexpr uint8_t kGt911BackupAddress = 0x14;
constexpr uint8_t kFt5x06Address = 0x38;
constexpr uint16_t kGt911ProductIdRegister = 0x8140;
constexpr uint16_t kGt911StatusRegister = 0x814E;
constexpr size_t kMaxTouchPoints = 5;
constexpr int kI2cTimeoutMs = 100;

}  // namespace

bool Touch::begin() {
  if (device_ != nullptr) {
    return true;
  }

  if (i2c_master_get_bus_handle(I2C_NUM_0, &bus_) != ESP_OK) {
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = static_cast<gpio_num_t>(kI2cSda),
        .scl_io_num = static_cast<gpio_num_t>(kI2cScl),
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd = false,
        },
    };
    const esp_err_t create_result = i2c_new_master_bus(&bus_config, &bus_);
    if (create_result != ESP_OK) {
      ESP_LOGE(kTag, "Create I2C bus failed: %s", esp_err_to_name(create_result));
      bus_ = nullptr;
      return false;
    }
  }

  // GT911 selects 0x5D or 0x14 from its INT/RST levels during reset. These
  // examples deliberately drive neither signal: probe both legal addresses,
  // then create the device with the address that actually acknowledged.
  if (i2c_master_probe(bus_, kGt911PrimaryAddress, kI2cTimeoutMs) == ESP_OK) {
    controller_ = TouchController::gt911;
    address_ = kGt911PrimaryAddress;
  } else if (i2c_master_probe(bus_, kGt911BackupAddress, kI2cTimeoutMs) == ESP_OK) {
    controller_ = TouchController::gt911;
    address_ = kGt911BackupAddress;
  } else if (i2c_master_probe(bus_, kFt5x06Address, kI2cTimeoutMs) == ESP_OK) {
    // The released LCD-3.5 board uses FT6336/FT5x06. Keep a polling fallback
    // so adding the requested GT911 variant does not regress released boards.
    controller_ = TouchController::ft5x06;
    address_ = kFt5x06Address;
  } else {
    ESP_LOGE(kTag, "No touch controller at GT911 0x5D/0x14 or FT5x06 0x38");
    controller_ = TouchController::none;
    address_ = 0;
    return false;
  }

  if (!open_device(address_)) {
    controller_ = TouchController::none;
    address_ = 0;
    return false;
  }

  if (controller_ == TouchController::gt911) {
    uint8_t product_id[4] = {};
    const esp_err_t result = read_register16(kGt911ProductIdRegister, product_id, sizeof(product_id));
    if (result != ESP_OK) {
      ESP_LOGE(kTag, "GT911 at 0x%02X did not expose its product ID: %s",
               address_, esp_err_to_name(result));
      i2c_master_bus_rm_device(device_);
      device_ = nullptr;
      controller_ = TouchController::none;
      address_ = 0;
      return false;
    }
    ESP_LOGI(kTag, "GT911 found at 0x%02X; INT/RST unused, polling enabled", address_);
  } else {
    ESP_LOGI(kTag, "FT5x06-compatible controller found at 0x%02X; polling enabled", address_);
  }
  return true;
}

bool Touch::open_device(uint8_t address) {
  const i2c_device_config_t device_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
      .scl_wait_us = 0,
      .flags = {
          .disable_ack_check = false,
      },
  };
  const esp_err_t result = i2c_master_bus_add_device(bus_, &device_config, &device_);
  if (result != ESP_OK) {
    ESP_LOGE(kTag, "Add touch device 0x%02X failed: %s", address, esp_err_to_name(result));
    device_ = nullptr;
    return false;
  }
  return true;
}

size_t Touch::read(TouchPoint *points, size_t capacity) {
  if (device_ == nullptr || points == nullptr || capacity == 0) {
    return 0;
  }
  capacity = std::min(capacity, kMaxTouchPoints);
  if (controller_ == TouchController::gt911) {
    return read_gt911(points, capacity);
  }
  if (controller_ == TouchController::ft5x06) {
    return read_ft5x06(points, capacity);
  }
  return 0;
}

size_t Touch::read_gt911(TouchPoint *points, size_t capacity) {
  uint8_t status = 0;
  if (read_register16(kGt911StatusRegister, &status, 1) != ESP_OK) {
    return 0;
  }

  const uint8_t reported_count = status & 0x0F;
  if ((status & 0x80) == 0 || reported_count == 0 || reported_count > kMaxTouchPoints) {
    if ((status & 0x80) != 0) {
      write_register16(kGt911StatusRegister, 0);
    }
    return 0;
  }

  uint8_t raw[kMaxTouchPoints * 8] = {};
  const size_t count = std::min<size_t>(reported_count, capacity);
  const esp_err_t read_result = read_register16(kGt911StatusRegister + 1, raw, reported_count * 8);
  const esp_err_t clear_result = write_register16(kGt911StatusRegister, 0);
  if (read_result != ESP_OK || clear_result != ESP_OK) {
    return 0;
  }

  for (size_t index = 0; index < count; ++index) {
    const size_t offset = index * 8;
    points[index].id = raw[offset];
    points[index].x = static_cast<uint16_t>(raw[offset + 1] | (raw[offset + 2] << 8));
    points[index].y = static_cast<uint16_t>(raw[offset + 3] | (raw[offset + 4] << 8));
    points[index].strength = static_cast<uint16_t>(raw[offset + 5] | (raw[offset + 6] << 8));
  }
  return count;
}

size_t Touch::read_ft5x06(TouchPoint *points, size_t capacity) {
  uint8_t raw[1 + kMaxTouchPoints * 6] = {};
  if (read_register8(0x02, raw, sizeof(raw)) != ESP_OK) {
    return 0;
  }

  const size_t count = std::min<size_t>(raw[0] & 0x0F, capacity);
  for (size_t index = 0; index < count; ++index) {
    const size_t offset = 1 + index * 6;
    points[index].x = static_cast<uint16_t>(((raw[offset] & 0x0F) << 8) | raw[offset + 1]);
    points[index].y = static_cast<uint16_t>(((raw[offset + 2] & 0x0F) << 8) | raw[offset + 3]);
    points[index].id = raw[offset + 2] >> 4;
    points[index].strength = raw[offset + 4];
  }
  return count;
}

esp_err_t Touch::read_register8(uint8_t reg, uint8_t *data, size_t length) {
  return i2c_master_transmit_receive(device_, &reg, 1, data, length, kI2cTimeoutMs);
}

esp_err_t Touch::read_register16(uint16_t reg, uint8_t *data, size_t length) {
  const uint8_t register_address[] = {
      static_cast<uint8_t>(reg >> 8),
      static_cast<uint8_t>(reg & 0xFF),
  };
  return i2c_master_transmit_receive(
      device_, register_address, sizeof(register_address), data, length, kI2cTimeoutMs);
}

esp_err_t Touch::write_register16(uint16_t reg, uint8_t value) {
  const uint8_t payload[] = {
      static_cast<uint8_t>(reg >> 8),
      static_cast<uint8_t>(reg & 0xFF),
      value,
  };
  return i2c_master_transmit(device_, payload, sizeof(payload), kI2cTimeoutMs);
}

const char *Touch::controller_name() const {
  switch (controller_) {
    case TouchController::gt911:
      return "GT911";
    case TouchController::ft5x06:
      return "FT5x06";
    default:
      return "none";
  }
}

}  // namespace lcd35
