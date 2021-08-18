#ifndef PIXELART_UTILS_REBOOT_H_
#define PIXELART_UTILS_REBOOT_H_

#include <Arduino.h>

void rebootWithMsg(const char* msg, uint32_t wait_time_before_reboot = 3000);

#endif