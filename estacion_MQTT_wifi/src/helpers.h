#ifndef helpers_h
#define helpers_h

#include "global.h"

float read_vin();
void show_lcd_sgp30();
void preparePayload(char* payload, int8_t rssi);
void setNTPTime();

#endif