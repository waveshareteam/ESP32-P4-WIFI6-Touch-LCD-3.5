#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

namespace lcd35 {

inline constexpr int kLcdMosi = 20;
inline constexpr int kLcdClock = 21;
inline constexpr int kLcdChipSelect = 23;
inline constexpr int kLcdDataCommand = 26;
inline constexpr int kLcdReset = 27;
inline constexpr int kLcdBacklight = 28;
inline constexpr int kLcdWidth = 320;
inline constexpr int kLcdHeight = 480;
inline constexpr uint32_t kLcdPixelClockHz = 80000000;

inline constexpr int kI2cSda = 7;
inline constexpr int kI2cScl = 8;

inline constexpr int kSdD0 = 39;
inline constexpr int kSdD1 = 40;
inline constexpr int kSdD2 = 41;
inline constexpr int kSdD3 = 42;
inline constexpr int kSdCommand = 44;
inline constexpr int kSdClock = 43;

inline constexpr int kI2sDataOut = 9;
inline constexpr int kI2sWordSelect = 10;
inline constexpr int kI2sDataIn = 11;
inline constexpr int kI2sBitClock = 12;
inline constexpr int kI2sMasterClock = 13;
inline constexpr int kPowerAmplifier = 53;

Arduino_GFX *display();
bool display_begin(uint8_t brightness_percent = 100);
void display_brightness(uint8_t brightness_percent);

}  // namespace lcd35
