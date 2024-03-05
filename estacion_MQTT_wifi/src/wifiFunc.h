
#ifndef wifiFunc_h
#define wifiFunc_h

#include <Arduino.h>
#include <PubSubClient.h>
#include "global.h"
#include "helpers.h"
#include "littlefsFunc.h"

void Wifi_control();
void wmSaveConfigCallback();
void sendMqtt(char* payload);
void Wifi_setSleep();

#endif