#include "global.h"

//leer los valores del BME280 y guardarlos
void read_bme() {
  bme_temp = bme.readTemperature();
  bme_hum = bme.readHumidity();
  bme_press = bme.readPressure() / 100.0F;
  bme_alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
}
//desactivar el BME280
void bme_sleep() {
  Wire.beginTransmission(BME_ADDR);
  Wire.write((uint8_t) 0xF4);
  Wire.write((uint8_t) 0x00);
  Wire.endTransmission();
}