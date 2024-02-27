
#ifndef wifiFunc_h
#define wifiFunc_h

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "global.h"
#include "helpers.h"
#include "littlefsFunc.h"

void Wifi_control();
void wmSaveConfigCallback();
void sendMqtt();
void Wifi_setSleep();

#endif