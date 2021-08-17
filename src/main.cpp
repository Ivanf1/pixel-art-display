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
uint32_t clientID = 0;

TFT_eSPI tft = TFT_eSPI();

uint8_t* bufferStore;

void storePixelVector(uint8_t* bufferStore) {
  fs::File img_file = SD.open("/img/file", "w");
  if (!img_file) {
    ESP_LOGE("", "could not open image file");
  }

  img_file.print((char*)bufferStore);

  img_file.close();
}

void drawPixel(unsigned int index, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t x = (index % 60) * 4;
  uint32_t y = (index / 60) * 4;

  tft.fillRect(x, y, 4, 4, tft.color565(r, g, b));
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
  // Serial.println("type");
  // Serial.println(type);
  // if (type == WS_EVT_DATA) {
  //   AwsFrameInfo* info = (AwsFrameInfo*)arg;
  //   Serial.println("final");
  //   Serial.println(info->final);
  //   Serial.println("len");
  //   Serial.println(len);
  //   if (info->final && info->index == 0 && info->len == len) {
  //     if (info->opcode == WS_TEXT) {
  //       data[len] = 0;
  //       // DeserializationError err = deserializeJson(doc, data, sizeof(doc));
  //       // if (err) {
  //       //   Serial.println("Error deserializing json");
  //       // } else {
  //       //   Serial.println("received");
  //       //   uint8_t r, g, b;
  //       //   r = doc["r"].as<uint8_t>();
  //       //   g = doc["g"].as<uint8_t>();
  //       //   b = doc["b"].as<uint8_t>();

  //       //   const char* fill = doc["fill"];
  //       //   const char* store = doc["store"];
  //       //   if (fill) {
  //       //     tft.fillScreen(tft.color565(r, g, b));
  //       //   } else if (store) {
  //       //     size_t size = JSON_ARRAY_SIZE(3600);
  //       //     Serial.println("size");
  //       //     Serial.println(size);
  //       //     // storePixelVector();
  //       //   } else {
  //       //     unsigned int index = doc["cellIdx"].as<unsigned int>();
  //       //     drawPixel(index, r, g, b);
  //       //   }
  //       // }
  //     }
  //   }
  // } else if (type == WS_EVT_CONNECT) {
  //   clientID = client->id();
  //   Serial.println("Client connected");
  // } else if (type == WS_EVT_DISCONNECT) {
  //   clientID = 0;
  //   Serial.println("Client disconnected");
  //   tft.fillScreen(TFT_WHITE);
  // }
  if(type == WS_EVT_CONNECT){
    ESP_LOGD("", "ws[%s][%u] connect\n", server->url(), client->id());
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    ESP_LOGD("", "ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    ESP_LOGD("", "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      ESP_LOGD("", "ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        ESP_LOGD("", "%s\n", (char*)data);
      }
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          ESP_LOGD("", "ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        ESP_LOGD("", "ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);

        bufferStore = (uint8_t*)pvPortMalloc((info->len) +1);
        if (!bufferStore) {
          ESP_LOGD("", "could not allocate buffer");
          return;
        }
        ESP_LOGD("", "buffer initialized");
      }

      ESP_LOGD("", "ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        memcpy(&bufferStore[info->index], data, len);
        // ESP_LOGD("", "%s\n", (char*)data);
      }

      if((info->index + len) == info->len){
        ESP_LOGD("", "ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        memcpy(&bufferStore[info->len], "\0", 1);
        if(info->final){
          ESP_LOGD("", "ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          storePixelVector(bufferStore);
          vPortFree(bufferStore);
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  if (!SD.begin()) {
    Serial.println("SD Card mount failed");
    return;
  }

  if (!SD.exists("/img")) {
    SD.mkdir("/img");
  }

  testSdCard();

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }
  Serial.println("WiFi connected\n");
  Serial.println(WiFi.localIP());

  tft.init();
  tft.fillScreen(TFT_WHITE);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();

  loadPixelVector();
}

void loop() {}