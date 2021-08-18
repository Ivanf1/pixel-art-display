#include "store/pixelVectorManager.h"

#include <Arduino.h>
#include <SPI.h>
#include "SD.h"
#include "FS.h"

void storePixelVector(uint8_t* bufferStore, const char* img_path) {
  fs::File img_file = SD.open(img_path, "w");
  if (!img_file) {
    ESP_LOGE(TAG, "could not open image file %s", img_path);
    return;
  }

  img_file.print((char*)bufferStore);

  img_file.close();
}

void loadPixelVector(drawPixel_t drawPixel, const char* img_path, uint16_t w, uint16_t h) {
  fs::File img_file = SD.open(img_path, "r");
  if (!img_file) {
    ESP_LOGE(TAG, "could not open image file %s", img_path);
    return;
  }

  uint32_t max = w * h;
  uint32_t i = 0;
  while (img_file.available() && i <= max) {
    long color = img_file.parseInt();
    drawPixel(i, color);
    i++;
  }

  img_file.close();
}