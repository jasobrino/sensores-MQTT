#include "global.h"

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire, OLED_RESET);
Adafruit_SGP30 sgp;
Adafruit_BME280 bme;

bool baseline_exists = false;
bool lcd_inst = false, SGP30_inst = false, BME280_inst=false, SGP30_calibration=false;
Config config;
esp_sleep_wakeup_cause_t wakeup_cause;
uint16_t CO2, TVOC, H2, ETH, eCO2_base, TVOC_base; // SGP30
float bme_hum, bme_temp, bme_press, bme_alt; //BME280
RTC_DATA_ATTR uint32_t bootCount = 0; //variable en ram no reseteada por deep sleep
ESP32Time rtc;

bool jumper_stat = false;
bool httpServer = false;
String flogname;
String SSID = "****";
String PASSWD = "****";
