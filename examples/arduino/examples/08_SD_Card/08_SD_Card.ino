/*
 * microSD card read/write demo for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.5.
 *
 * The microSD slot is wired to the SDIO 3.0 interface:
 *   CLK=GPIO43, CMD=GPIO44, D0=GPIO39, D1=GPIO40, D2=GPIO41, D3=GPIO42
 */
#include <Arduino.h>
#include <SD_MMC.h>
#include <FS.h>
#include <lcd35_board.h>

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!SD_MMC.setPins(lcd35::kSdClock, lcd35::kSdCommand, lcd35::kSdD0,
                      lcd35::kSdD1, lcd35::kSdD2, lcd35::kSdD3)) {
    Serial.println("SD_MMC pin configuration failed");
    return;
  }
  if (!SD_MMC.begin("/sdcard", false /* 4-bit */)) {
    Serial.println("Card mount failed - insert a FAT32-formatted microSD card");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  Serial.printf("Card type: %s\n",
    cardType == CARD_MMC ? "MMC" : cardType == CARD_SD ? "SDSC" : cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
  Serial.printf("Card size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
  Serial.printf("Total space: %llu MB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %llu MB\n", SD_MMC.usedBytes() / (1024 * 1024));

  const char *path = "/hello_lcd35.txt";
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open file for writing");
    return;
  }
  f.println("Hello from ESP32-P4-WIFI6-Touch-LCD-3.5 microSD demo!");
  f.close();
  Serial.println("Wrote /hello_lcd35.txt");

  f = SD_MMC.open(path, FILE_READ);
  if (f) {
    Serial.println("Read back:");
    while (f.available()) {
      Serial.write(f.read());
    }
    f.close();
  }

  Serial.println("Listing root directory:");
  File root = SD_MMC.open("/");
  File entry = root.openNextFile();
  while (entry) {
    Serial.printf("  %s (%u bytes)\n", entry.name(), (unsigned)entry.size());
    entry = root.openNextFile();
  }
  root.close();
  Serial.println("SD card demo finished");
}

void loop() {
  delay(1000);
}
