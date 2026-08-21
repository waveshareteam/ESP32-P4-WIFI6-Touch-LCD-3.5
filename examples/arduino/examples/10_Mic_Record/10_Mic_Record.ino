/*
 * ES8311 analog microphone capture demo for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5.
 *
 * Captures the on-board analog microphone through the ES8311 codec (I2C 0x18) and prints
 * the recorded PCM data to the serial monitor: per 30 ms frame the peak, RMS
 * and 16 decimated 16-bit samples are printed as comma-separated values.
 *
 * I2C: SDA=GPIO7, SCL=GPIO8. I2S: MCLK=GPIO13, BCLK=GPIO12, LRCK=GPIO10,
 * DIN=GPIO11.
 */
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>
#include "esp_check.h"
#include "es8311.h"
#include <lcd35_board.h>

#define SAMPLE_RATE      16000
#define FRAME_LENGTH_MS  30
#define FRAME_SAMPLES    (FRAME_LENGTH_MS * SAMPLE_RATE / 1000)
#define I2S_CH           I2S_NUM_0

static int16_t *frame = NULL;

esp_err_t es8311_codec_init(void) {
  es8311_handle_t es_handle = es8311_create(0, ES8311_ADDRRES_0);
  ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, "ES8311", "create failed");
  const es8311_clock_config_t es_clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = SAMPLE_RATE * 256,
    .sample_frequency = SAMPLE_RATE,
  };
  ESP_RETURN_ON_ERROR(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16,
                                  ES8311_RESOLUTION_16),
                      "ES8311", "init failed");
  ESP_RETURN_ON_ERROR(es8311_microphone_config(es_handle, false), "ES8311",
                      "analog microphone configuration failed");
  return es8311_microphone_gain_set(es_handle, ES8311_MIC_GAIN_24DB);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(lcd35::kI2cSda, lcd35::kI2cScl, 100000);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_16BIT,
  };
  i2s_pin_config_t pin_config = {
    .mck_io_num = lcd35::kI2sMasterClock,
    .bck_io_num = lcd35::kI2sBitClock,
    .ws_io_num = lcd35::kI2sWordSelect,
    .data_out_num = -1,
    .data_in_num = lcd35::kI2sDataIn,
  };
  esp_err_t ret = i2s_driver_install(I2S_CH, &i2s_config, 0, NULL);
  if (ret != ESP_OK) {
    Serial.printf("I2S driver install failed: %s\n", esp_err_to_name(ret));
    return;
  }
  ret = i2s_set_pin(I2S_CH, &pin_config);
  if (ret != ESP_OK) {
    Serial.printf("I2S pin config failed: %s\n", esp_err_to_name(ret));
    i2s_driver_uninstall(I2S_CH);
    return;
  }
  i2s_zero_dma_buffer(I2S_CH);

  if (es8311_codec_init() != ESP_OK) {
    Serial.println("ES8311 analog microphone init failed!");
    i2s_driver_uninstall(I2S_CH);
    return;
  }
  frame = (int16_t *)malloc(FRAME_SAMPLES * sizeof(int16_t));
  if (frame == NULL) {
    Serial.println("frame buffer allocation failed!");
    i2s_driver_uninstall(I2S_CH);
    return;
  }
  Serial.println("ES8311 analog microphone ready; printing PCM frames");
}

void loop() {
  if (frame == NULL) {
    delay(1000);
    return;
  }
  size_t bytes_read = 0;
  esp_err_t ret = i2s_read(I2S_CH, (char *)frame, FRAME_SAMPLES * sizeof(int16_t), &bytes_read, portMAX_DELAY);
  if (ret != ESP_OK || bytes_read != FRAME_SAMPLES * sizeof(int16_t)) {
    Serial.printf("I2S read failed: %s, bytes: %u\n", esp_err_to_name(ret), (unsigned)bytes_read);
    delay(5);
    return;
  }

  int32_t peak = 0;
  int64_t sum_sq = 0;
  for (int i = 0; i < FRAME_SAMPLES; i++) {
    int32_t v = frame[i];
    if (v < 0) v = -v;
    if (v > peak) peak = v;
    sum_sq += (int64_t)frame[i] * frame[i];
  }
  int32_t rms = (int32_t)sqrtf((float)sum_sq / FRAME_SAMPLES);

  Serial.print(millis());
  Serial.print(" peak=");
  Serial.print(peak);
  Serial.print(" rms=");
  Serial.print(rms);
  Serial.print(" samples=");
  // decimated dump: 16 samples per frame
  for (int i = 0; i < FRAME_SAMPLES; i += FRAME_SAMPLES / 16) {
    Serial.print(frame[i]);
    Serial.print(",");
  }
  Serial.println();
}
