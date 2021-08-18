#ifndef PIXELART_STORE_PIXELVECTORMANAGER_H_
#define PIXELART_STORE_PIXELVECTORMANAGER_H_

#include <Arduino.h>

typedef void (*drawPixel_t)(unsigned int index, uint16_t color);

void storePixelVector(uint8_t* bufferStore, const char* img_path);
void loadPixelVector(drawPixel_t drawPixel, const char* img_path, uint16_t w, uint16_t h);

#endif