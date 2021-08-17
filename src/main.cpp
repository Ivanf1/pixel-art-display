#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "SD.h"
#include "FS.h"

#include "secrets.h"
#include "utils/sdCardTest.h"

StaticJsonDocument<256> doc;

AsyncWebServer server(80);
AsyncWebSocket ws("/display");
uint8_t clientID = 0;

TFT_eSPI tft = TFT_eSPI();

uint8_t* bufferStore = NULL;

void storePixelVector(uint8_t* bufferStore) {
  fs::File img_file = SD.open("/img/file", "w");
  if (!img_file) {
    ESP_LOGE("", "could not open image file");
  }

  img_file.print((char*)bufferStore);

  img_file.close();
}

void drawPixel(unsigned int index, uint16_t color) {
  uint32_t x = (index % 60) * 4;
  uint32_t y = (index / 60) * 4;

  tft.fillRect(x, y, 4, 4, color);
}

void loadPixelVector() {
  fs::File img_file = SD.open("/img/file", "r");
  if (!img_file) {
    ESP_LOGE("", "could not open image file");
  }

  unsigned int max = 60 * 60;
  unsigned int i = 0;
  while (img_file.available() && i <= max) {
    long color = img_file.parseInt();
    drawPixel(i, color);
    i++;
  }

  img_file.close();
}

void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      if (info->opcode == WS_TEXT) {
        data[len] = 0;
        DeserializationError err = deserializeJson(doc, data, sizeof(doc));
        if (err) {
          ESP_LOGE("", "Error deserializing json on ws received");
        } else {
          uint16_t color = doc["color"].as<uint16_t>();
          if (!color) {
            ESP_LOGE("", "Invalid color from json ws");
            return;
          }

          const char* fill = doc["fill"];
          if (fill) {
            tft.fillScreen(color);
          } else {
            unsigned int index = doc["cellIdx"].as<unsigned int>();
            drawPixel(index, color);
          }
        }
      }
    } else {
      // message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0) {
        if (info->num == 0) {
          ESP_LOGD("", "ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        }
        ESP_LOGD("", "ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);

        bufferStore = (uint8_t*)pvPortMalloc((info->len) +1);
        if (!bufferStore) {
          ESP_LOGD("", "could not allocate pixel buffer");
          return;
        }
        ESP_LOGD("", "pixel buffer initialized");
      }

      ESP_LOGD("", "ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT) {
        data[len] = 0;
        if (bufferStore) {
          memcpy(&bufferStore[info->index], data, len);
        }
      }

      if ((info->index + len) == info->len) {
        ESP_LOGD("", "ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (bufferStore) {
          memcpy(&bufferStore[info->len], '\0', 1);
        }
        if (info->final) {
          ESP_LOGD("", "ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (bufferStore) {
            storePixelVector(bufferStore);
            vPortFree(bufferStore);
          }
        }
      }
    }
  } else if (type == WS_EVT_CONNECT) {
    clientID = client->id();
    ESP_LOGD("", "ws[%s][%u] connect\n", server->url(), clientID);
  } else if (type == WS_EVT_DISCONNECT) {
    clientID = 0;
    ESP_LOGD("", "ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR) {
    ESP_LOGD("", "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  if (!SD.begin()) {
    ESP_LOGE("", "SD Card mount failed");
    return;
  }

  if (!SD.exists("/img")) {
    ESP_LOGD("", "img dir does not exist");
    if (SD.mkdir("/img")) {
      ESP_LOGD("", "img dir created");
    } else {
      ESP_LOGE("", "could not create img dir");
    }
  }

  // testSdCard();

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    return;
  }

  tft.init();
  tft.fillScreen(TFT_WHITE);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();

  loadPixelVector();
}

void loop() {}