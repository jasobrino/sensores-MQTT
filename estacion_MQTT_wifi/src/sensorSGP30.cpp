
#include "global.h"
#include "sensorSGP30.h"
#include "helpers.h"
//lectura de valores del SGP30
bool SGP30_read_values() {
  //primero realizamos un tiempo de lecturas del sensor hasta que se estabilize
  uint8_t num_lect = 20;
  for(uint8_t i=0; i < num_lect; i++) {
    delay(1000); // 1 seg entre lecturas
    if(!sgp.IAQmeasure()) {
      Serial.println("error en sgp.IAQmeasure");
      return false;
    }
    CO2 = sgp.eCO2;
    TVOC = sgp.TVOC;
    Serial.printf("eCO2: %03u  TVOC: %03u  lect:%u\n", CO2, TVOC, i + 1);
    if (!sgp.IAQmeasureRaw()) {
      Serial.println("\nerror en sgp.IAQmeasureRaw");
    }
    H2 = sgp.rawH2;
    ETH = sgp.rawEthanol;
    Serial.printf("H2: %05u  Eth: %05u  lect:%u\n", H2, ETH, i + 1);
  }
  return true;
}

//recupera los valores eCO2 y TVOC almacenados, si no están pasamos a modo calibración
void set_baseline_values() {
  if(LittleFS.exists("/TVOC") && LittleFS.exists("/eCO2")) {
    f = LittleFS.open("/TVOC");
    TVOC_base = f.readString().toInt();
    f.close();
    f = LittleFS.open("/eCO2");
    eCO2_base = f.readString().toInt();
    f.close();
    sgp.IAQinit();
    if(sgp.setIAQBaseline(eCO2_base, TVOC_base)) {
      Serial.printf("** recuperado TVOC: %04X  y eCO2: %04X\n", TVOC_base, eCO2_base);
    } else {
      Serial.printf("error en sgp.setIAQBaseline(%u, %u)", eCO2_base, TVOC_base);
    }
  } else {
    //activamos el indicador de calibración para el SGP30
    SGP30_calibration = true;
  }
}

//cálculo de la humedad absoluta - necesario para el SGP30
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}

//desactivar el SGP30
void spg30_sleep() {
  Wire.beginTransmission(0x00); // General Call mode -> writing to address 0 creates a 0x00 first byte
  Wire.write(0x06);
  Wire.endTransmission();
}

//calibracion valores baseline para SGP30
void SGP30Calibration() {
  //ahora debemos calcular los valores de baseline del SGP30
  // los primeros 10 minutos en el exterior, el proceso tarda 12 horas
  uint32_t seg_12h = 60 * 60 * 12;
  for(uint32_t i = seg_12h; i > 0; i--) {
    delay(1000);
    sgp.IAQmeasure();
    sgp.IAQmeasureRaw();
    display.clearDisplay();
    display.setCursor(0,10);
    if (!sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Error al leer baseline");
      display.println("error en getIAQBaseline");
    } else {
      display.printf("eCO2: %04X TVOC: %04X", eCO2_base, TVOC_base);
    }
    display.setCursor(0,20);
    display.printf("faltan %u v:%.2f", i, read_vin());
    display.display();
    Serial.printf("calibrando baseline: eCO2: %04X TVOC: %04X  - %u\n", eCO2_base, TVOC_base, i);
  }
  //guardamos los valores capturados en los archivos
  f = LittleFS.open("/TVOC", "w");
  f.print(String(TVOC_base));
  f.close();
  f = LittleFS.open("/eCO2", "w");
  f.print(String(eCO2_base));
  f.close();
  Serial.println("*** calibración terminada ***");
  ESP.restart();
}


