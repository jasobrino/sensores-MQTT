
#ifndef global_h
#define global_h

#include <Arduino.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_BME280.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Time.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
// #include <ESPmDNS.h>


#define OLED_RESET -1
#define uS_TO_S_FACTOR 1000000ULL
#define SEC_SLEEP_DEFAULT 60
#define PIN_ADC 36 //se corresponde con SVP
#define BME_ADDR 0x76 //con SDO a Vcc pasa a 0x77
#define SEALEVELPRESSURE_HPA 1013.25 //valor por defecto
#define PIN_LED 12
#define PIN_JMP_IN  32 // jumper para indicar modo funcionamiento
#define PIN_JMP_OUT 33
/* 
using RichHttpConfig = RichHttp::Generics::Configs::EspressifBuiltin;
using RequestContext = RichHttpConfig::RequestContextType; */

extern Adafruit_SGP30 sgp;
extern Adafruit_SSD1306 display;
extern Adafruit_BME280 bme;

extern uint16_t CO2, TVOC, H2, ETH, eCO2_base, TVOC_base; // SGP30
extern bool baseline_exists;
extern float bme_hum, bme_temp, bme_press, bme_alt; //BME280
extern bool lcd_inst , SGP30_inst, BME280_inst, SGP30_calibration;
extern esp_sleep_wakeup_cause_t wakeup_cause;
extern RTC_DATA_ATTR uint32_t bootCount; //variable en ram no reseteada por deep sleep
extern bool jumper_stat;
extern bool httpServer;
// extern ESP32WebServer server;
extern String SSID;
extern String PASSWD;

/* using RichHttpConfig = RichHttp::Generics::Configs::EspressifBuiltin;
using RequestContext = RichHttpConfig::RequestContextType;
extern RichHttpServer<RichHttpConfig> server; */


struct Config {
  char MQTT_HOST[40];
  uint16_t MQTT_PORT;
  char ID[20];
  uint16_t SEC_SLEEP;
};

extern Config config;
extern ESP32Time rtc;
extern String flogname;

#endif
