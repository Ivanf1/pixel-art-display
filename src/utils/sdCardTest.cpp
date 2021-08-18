#include <Arduino.h>
#include "SD.h"
#include "FS.h"
#include <SPI.h>

void testSdCard() {
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE) {
    ESP_LOGD(TAG, "No SD card attached");
    return;
  }

  ESP_LOGD(TAG, "SD Card Type: ");
  if (cardType == CARD_MMC) {
    ESP_LOGD(TAG, "MMC");
  } else if(cardType == CARD_SD) {
    ESP_LOGD(TAG, "SDSC");
  } else if(cardType == CARD_SDHC) {
    ESP_LOGD(TAG, "SDHC");
  } else {
    ESP_LOGD(TAG, "UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  ESP_LOGD(TAG, "SD Card Size: %lluMB", cardSize);
}