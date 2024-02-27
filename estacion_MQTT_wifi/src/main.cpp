
#include "soc/soc.h" 
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include "global.h"
#include "wifiFunc.h"
#include "sensorSGP30.h"
#include "sensorBME280.h"

uint32_t lastTime = 0;

// declaración funciones
void inicializacion();

void setup()
{
  //preparamos entorno
  inicializacion();
  // control motivo wakeup
  wakeup_cause = esp_sleep_get_wakeup_cause();
  switch(wakeup_cause) {
    //entrada por timer
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("wakeup causado por TIMER");
      break;
    //al arrancar o hacer reset se entra en default
    default: 
      Serial.println("wakeup no causado por deep sleep");
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.clearDisplay();
      display.setCursor(0,0);
      display.printf("%s RESTART", config.ID);
      if(SGP30_inst) {
        //ahora asignamos los valores de baseline guardados
        set_baseline_values();
        //no se han podido abrir los archivos de baseline
        if(SGP30_calibration) {
          //ahora debemos calcular los valores de baseline del SGP30
          // los primeros 10 minutos en el exterior, el proceso tarda 12 horas
          SGP30Calibration();
          Serial.println("calculando baseline (12h)");
        }
        //mostramos valores
        display.setCursor(0,10);
        display.printf("eCO2: %u", eCO2_base);
        display.setCursor(0,20);
        display.printf("TVOC: %u", TVOC_base);
        display.display();
      }
  }
  // se procede a capturar datos y enviarlos
  if(SGP30_inst) {
    if(!SGP30_read_values()) {
      Serial.printf("error en SGP30_read_values");
    }
    Serial.printf("TVOC: %03u  eCO2: %03u  bootCount: %u\n",TVOC, CO2, bootCount);
  }   
  if(BME280_inst) {
    Serial.printf("BME280: temp:%.2fC hum:%.2f%% press:%.2f hPa Alt:%.2f m\n",
                  bme_temp, bme_hum, bme_press, bme_alt);
  }
  if(lcd_inst) show_lcd_sgp30();
  bootCount++;
  Serial.printf("Tiempo lectura sensores: %u ms\n", millis() - lastTime);
  lastTime = millis();

  if(SGP30_inst) spg30_sleep(); //apagamos el SGP30 (soft reset)
  if(BME280_inst) bme_sleep(); //reducimos el consumo del BME280
  //activamos el wifi y enviamos los datos por mqtt
  // if(esp_wifi_start() == ESP_OK) Serial.println("wifi restablecido");
  Wifi_control();
  // delay(5000); //para medir consumo
  Serial.printf("Tiempo conexión wifi: %u ms\n", millis() - lastTime);
  // pasamos a modo deep sleep
  Serial.flush();
  if(lcd_inst) display.ssd1306_command(SSD1306_DISPLAYOFF); //apagar lcd
  Serial.println("**** deep_sleep ****\n");
  esp_deep_sleep_start();
} 

void loop() {}

// configuramos puertos e inicalizamos sensores
void inicializacion() {
  lastTime = millis();
  Serial.begin(115200);
  Serial.println("\n-----------------------------------------------");
  pinMode(PIN_LED, OUTPUT);

  //ESP32 - desactiva brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  // WiFi.mode(WIFI_STA); //modo station por defecto
  //bajamos la frecuencia de la cpu
  bool changeFreq = setCpuFrequencyMhz(80);
  delay(50);
  Serial.flush();
  if(changeFreq) {
    Serial.printf("\nfreq actual   CPU: %u\n", getCpuFrequencyMhz());
  } else Serial.println("error en setCpuFrecuencyMhz()");
  //descativar wifi
   Wifi_setSleep();
  // if(esp_wifi_stop() == ESP_OK) Serial.println("wifi_stop ok");
  //capturamos la configuración guardada en LittleFS
  get_config(); 
  //configuración del tiempo de sleep
  esp_sleep_enable_timer_wakeup(config.SEC_SLEEP * uS_TO_S_FACTOR);
  // iniciando dispositivos y sensores
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.setRotation(2); //rotación display 180 grados
    lcd_inst = true;
  }
  if (sgp.begin()) {
    SGP30_inst = true;
    Serial.print("SGP30 serial #");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);
    // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
    //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!
  }
  //sensor BME280
  if(!bme.begin(BME_ADDR)) {
    Serial.println("no se ha encontrado BME280");
    BME280_inst = false;
  } else {
    //leer datos del sensor
    read_bme();
    BME280_inst = true;
    if(SGP30_inst) {
      //ahora capturamos la humedad absoluta con los valores del bme280
      uint32_t ABS_Humidity = getAbsoluteHumidity( bme_temp, bme_hum);
      sgp.setHumidity(ABS_Humidity);
      Serial.printf("Humedad absoluta: %u\n", ABS_Humidity);
    }
  }

}