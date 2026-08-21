/*
 * OV5647 MIPI-CSI ISP / 3A tuning demo for the Waveshare
 * ESP32-P4-WIFI6-Touch-LCD-3.5 (320x480 ST7796 display).
 *
 * Live preview plus interactive sensor/ISP controls through the serial
 * monitor. The ISP pipeline of the ESP_Video library exposes the common
 * 3A knobs through V4L2 extended controls:
 *
 *   g <0..1023>   sensor analog gain
 *   e <0..10000>  sensor exposure time (us)
 *   a <0..255>    AE target level
 *   v <0|1>       vertical flip
 *   h <0|1>       horizontal flip
 *   t <0|1>       sensor test pattern
 *   s             print current settings
 *
 * Example: "g 128" doubles the brightness, "a 64" brightens the AE target.
 * Values are forwarded to the driver only when they change; the HUD on the
 * display shows the last accepted settings.
 */
#ifndef BOARD_HAS_PSRAM
#error "This program requires PSRAM enabled (enable PSRAM in the Tools menu)"
#endif

#include <ESP_Video.h>
#include "lcd35_board.h"
#include "serial_log.h"

#define CAMERA_SCCB_PORT 0
#define CAMERA_SCCB_SCL  8
#define CAMERA_SCCB_SDA  7

ESPVideoClass video;
ESPVideoCaptureDevClass capture_dev;
const size_t kCaptureBufferCount = 2;

Arduino_GFX *gfx = lcd35::display();

struct IspSettings {
  int32_t gain = 0;         // 0 = driver default
  int32_t exposure = 0;     // 0 = driver default
  int32_t ae_target = 0;    // 0 = driver default
  int32_t vflip = -1;
  int32_t hflip = -1;
  int32_t test_pattern = -1;
} settings;

static bool applySetting(char op, long value) {
  switch (op) {
    case 'g':
      if (value < 0 || value > 1023 || !capture_dev.setSensorGain(value)) return false;
      settings.gain = value;
      return true;
    case 'e':
      if (value < 0 || value > 10000 || !capture_dev.setSensorExposureTime(value)) return false;
      settings.exposure = value;
      return true;
    case 'a':
      if (value < 0 || value > 255 || !capture_dev.setSensorAETargetLevel(value)) return false;
      settings.ae_target = value;
      return true;
    case 'v':
      if ((value != 0 && value != 1) || !capture_dev.setSensorVFlip(value != 0)) return false;
      settings.vflip = value;
      return true;
    case 'h':
      if ((value != 0 && value != 1) || !capture_dev.setSensorHFlip(value != 0)) return false;
      settings.hflip = value;
      return true;
    case 't':
      if ((value != 0 && value != 1) || !capture_dev.setSensorTestPattern(value != 0)) return false;
      settings.test_pattern = value;
      return true;
    default:
      return false;
  }
}

static void showSettings() {
  serial_log::printf(
    "gain=%ld exposure_us=%ld ae_target=%ld vflip=%ld hflip=%ld test_pattern=%ld\n",
    (long)settings.gain, (long)settings.exposure, (long)settings.ae_target,
    (long)settings.vflip, (long)settings.hflip, (long)settings.test_pattern);
}

static void handleSerial() {
  static String cmd;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      cmd.trim();
      if (cmd.length() > 0) {
        const char op = cmd[0];
        if (op == 's' && cmd.length() == 1) {
          showSettings();
        } else {
          const char *value_text = cmd.length() > 2 ? cmd.c_str() + 2 : "";
          char *end = nullptr;
          const long value = strtol(value_text, &end, 10);
          if (cmd.length() > 2 && cmd[1] == ' ' && end != value_text &&
              *end == '\0' && applySetting(op, value)) {
            showSettings();
          } else {
            serial_log::printf("rejected command: %s\n", cmd.c_str());
          }
        }
      }
      cmd = "";
    } else {
      cmd += c;
    }
  }
}

void setup() {
  serial_log::begin(115200);

  if (!lcd35::display_begin()) {
    serial_log::println("display begin failed!");
    return;
  }
  gfx->fillScreen(RGB565_BLACK);

  ESPVideoCamConfigClass cam_config;
  cam_config.begin(CAMERA_SCCB_PORT, CAMERA_SCCB_SCL, CAMERA_SCCB_SDA);
  ESPVideoCSIConfigClass csi_config;
  csi_config.begin(cam_config);
  if (!video.begin(csi_config) ||
      !capture_dev.begin(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, kCaptureBufferCount) ||
      !capture_dev.setFormat(ESP_VIDEO_FORMAT_RGB565) ||
      !capture_dev.startCapture()) {
    serial_log::println("camera pipeline init failed");
    return;
  }
  serial_log::println("ISP tuning ready. Commands: g/e/a/v/h/t/s (see sketch header)");
  showSettings();
}

void loop() {
  handleSerial();

  if (!capture_dev.isOpened() || !capture_dev.isCaptureStarted()) {
    delay(500);
    return;
  }
  ESPVideoBufferClass buffer = capture_dev.captureBuffer();
  if (!buffer.valid()) {
    delay(5);
    return;
  }
  uint32_t w = buffer.getWidth();
  uint32_t h = buffer.getHeight();
  if (w > 0 && h > 0 && buffer.formatType() == ESP_VIDEO_FORMAT_RGB565) {
    int16_t src_x = (int32_t)w > gfx->width() ? (w - gfx->width()) / 2 : 0;
    int16_t src_y = (int32_t)h > gfx->height() ? (h - gfx->height()) / 2 : 0;
    int16_t dst_x = (int32_t)w < gfx->width() ? (gfx->width() - w) / 2 : 0;
    int16_t dst_y = (int32_t)h < gfx->height() ? (gfx->height() - h) / 2 : 0;
    int16_t draw_w = w <= gfx->width() ? w : gfx->width();
    int16_t draw_h = h <= gfx->height() ? h : gfx->height();
    const uint16_t *pixels = (const uint16_t *)buffer.data();
    for (int16_t y = 0; y < draw_h; y++) {
      gfx->draw16bitRGBBitmap(dst_x, dst_y + y,
                              (uint16_t *)(pixels + (size_t)(src_y + y) * w + src_x), draw_w, 1);
    }

    // HUD overlay
    gfx->setTextColor(RGB565_BLACK, RGB565_WHITE);
    gfx->setCursor(4, 4);
    gfx->printf("gain=%ld exp=%ldus ae=%ld", (long)settings.gain, (long)settings.exposure, (long)settings.ae_target);
  }
}
