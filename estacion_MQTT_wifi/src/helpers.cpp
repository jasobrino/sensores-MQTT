#include "helpers.h"

// lectura estado bateria por el ADC
float read_vin() {
  #define FACT_CORR 1.73
  uint16_t vin = analogRead(PIN_ADC);
  float ADCv = (((vin * 3.3) / 4095) + 0.1) * FACT_CORR;
  return ADCv;
}

//mostrar información en el lcd
void show_lcd_sgp30() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  String now = rtc.getTime("%02d/%02m/%Y");
  if(SGP30_inst) {
    display.setCursor(0,0);
    display.printf("eCO2:%03u TVOC:%03u", CO2, TVOC);
    // display.setCursor(0,10);
    // display.printf("eCO2:%4X TVOC:%4X", eCO2_base, TVOC_base);
  }
  if(BME280_inst) {
    display.setCursor(0,10);
    display.printf("T:%.2fC H:%.2f%%", bme_temp, bme_hum);
  }
  display.setCursor(0,20);
  // display.printf("lp:%07u V:%.2f", bootCount, read_vin());
  display.printf("lp:%06u %s", bootCount, now.c_str());
  display.display();
}

//prepara los datos de la estación a formato JSON
void preparePayload(char* payload,  int8_t rssi) {
  if(WiFi.isConnected()) {
    Serial.println("preparePayload: wifi connected");
    setNTPTime();
  }
  char buffer[100];
  //snprintf(payload, sizeof(payload), "{ \"ID\": \"%s\"", config.ID);
  sprintf(payload, "{ \"ID\": \"%s\"", config.ID);
  //añadimos datos de los sensores
  if(BME280_inst) {
    char str_hum[10], str_temp[10], str_press[10];
    dtostrf(bme_hum, 6, 2, str_hum);    
    dtostrf(bme_temp, 6, 2, str_temp);
    dtostrf(bme_press, 8, 2, str_press);
    snprintf(buffer,  sizeof(buffer), ", \"TEMP\": %s, \"HUMEDAD\": %s, \"PRESION\": %s", str_temp, str_hum, str_press);
    strcat(payload, buffer);
  }
  if(SGP30_inst) {
    snprintf(buffer, sizeof(buffer), ", \"eCO2\": %u, \"TVOC\": %u", CO2, TVOC);
    strcat(payload, buffer);
  }
  char str_vbat[10];
  dtostrf(read_vin(), 4, 2, str_vbat);
  //con el jumper puesto no se envía el rssi pero si la fecha
  if(jumper_stat) {
    snprintf(buffer,  sizeof(buffer), ", \"VBAT\": %s", str_vbat);
    //añadimos al payload la fecha y hora del rtc
    String now = rtc.getTime("%02d/%02m/%Y %02H:%02M:%02S");
    snprintf(buffer, sizeof(buffer), ", \"date\": \"%s\" }", now.c_str());
  } else {
    snprintf(buffer,  sizeof(buffer), ", \"VBAT\": %s, \"RSSI\": %d }", str_vbat, rssi);
  }
  strcat(payload, buffer);
}

//capturamos la hora del servidor NTP y la grabamos en el RTC del esp32
void setNTPTime() {
  const char* ntpServer = "pool.ntp.org";
  const long gmtOffsect_sec = 3600; //1 hora: GMT+1
  const int  daylightOffset_sec = 3600;

  configTime(gmtOffsect_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
  }
  Serial.printf("setNTPTime: %s\n", rtc.getTime("%02d/%02m/%Y %02H:%02M:%02S").c_str());
}
