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
  display.printf("lp:%07u V:%.2f", bootCount, read_vin());
  display.display();
}