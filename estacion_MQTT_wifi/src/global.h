
#ifndef global_h
#define global_h

#include <Arduino.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_BME280.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
/* #include "wifiFunc.h"
#include "littlefsFunc.h"
#include "helpers.h" */

#define OLED_RESET -1
#define uS_TO_S_FACTOR 1000000ULL
#define SEC_SLEEP_DEFAULT 60
#define PIN_ADC 36 //se corresponde con SVP
#define BME_ADDR 0x76 //con SDO a Vcc pasa a 0x77
#define SEALEVELPRESSURE_HPA 1013.25 //valor por defecto
#define PIN_LED 12

extern Adafruit_SGP30 sgp;
extern Adafruit_SSD1306 display;
extern Adafruit_BME280 bme;

extern uint16_t CO2, TVOC, H2, ETH, eCO2_base, TVOC_base; // SGP30
extern bool baseline_exists;
extern File f;
extern float bme_hum, bme_temp, bme_press, bme_alt; //BME280
extern bool lcd_inst , SGP30_inst, BME280_inst, SGP30_calibration;
extern esp_sleep_wakeup_cause_t wakeup_cause;
extern RTC_DATA_ATTR uint32_t bootCount; //variable en ram no reseteada por deep sleep

struct Config {
  char MQTT_HOST[40];
  uint16_t MQTT_PORT;
  char ID[20];
  uint16_t SEC_SLEEP;
};

extern Config config;

#endif
