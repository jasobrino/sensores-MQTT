
#include "soc/soc.h" 
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include "global.h"
#include "wifiFunc.h"
#include "sensorSGP30.h"
#include "sensorBME280.h"
#include "httpFlashServer.h"

uint32_t lastTime = 0;
// declaración funciones
void inicializacion();
void sendDataSensors();

void setup() {
  //preparamos entorno
  inicializacion();
  //sin no hay jumper, comprobar si existe fichero log
  if(!jumper_stat && fileExists(flogname)) {
    //ahora debemos arrancar el servidor web
    httpServer = true;
    Wifi_control(); //conexión a router
    Serial.println("arrancando servidor web...");
    startServer();
    IPAddress ip = WiFi.localIP();
    Serial.printf("\nconectado a wifi: %d.%d.%d.%d\n", ip[0],ip[1],ip[2],ip[3]);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(0,0);
    display.printf("%s HTTP server", config.ID);
    display.setCursor(0,10);
    display.printf("HTTP: %d.%d.%d.%d", ip[0],ip[1],ip[2],ip[3]);
    display.display();
  } else {
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
      
        // en este punto, la estación comienza a recopilar los datos
        Serial.println("leyendo datos de los sensores...");
        if(SGP30_inst) {
          //ahora asignamos los valores de baseline guardados
          set_baseline_values();
          //no se han podido abrir los archivos de baseline
          if(SGP30_calibration) {
            //ahora debemos calcular los valores de baseline del SGP30
            // los primeros 10 minutos en el exterior, el proceso tarda 12 horas
            Serial.println("calculando baseline (12h)");
            SGP30Calibration();
          }
          //mostramos valores
          String now = rtc.getTime("%02d/%02m/%Y %02H:%02M:%02S");
          display.setCursor(0,10);
          display.printf("CO2:%u TV:%u", eCO2_base, TVOC_base);
          display.setCursor(0,20);
          display.printf(now.c_str());
          display.display();
        }
    }
    // se procede a capturar datos y enviarlos
    sendDataSensors();
    // pasamos a modo deep sleep
    Serial.flush();
    if(lcd_inst) display.ssd1306_command(SSD1306_DISPLAYOFF); //apagar lcd
    Serial.println("**** deep_sleep ****\n");
    esp_deep_sleep_start();
  } 
}

void loop() {
  //comprobamos si el servidor web está en funcionamiento
  if(httpServer) {
      // server.handleClient();
      serverHandleClient();
  }
}

void sendDataSensors() {
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
  
  //según la posición del jumper, se guardan datos en la flash o se envían por wifi
  char payload[200];
  if(jumper_stat) {
    // modo de datos locales
    preparePayload(payload, 0); //cargamos el payload con los datos
    Serial.printf("(datos locales) payload: %s\n", payload);
    saveDatalog(payload);
  } else {
    //activamos el wifi y enviamos los datos por mqtt
    // if(esp_wifi_start() == ESP_OK) Serial.println("wifi restablecido");
    Wifi_control();
    setNTPTime(); //ajustamos primero fecha y hora con el servidor NTP
    preparePayload(payload, WiFi.RSSI()); //cargamos el payload con los datos
    sendMqtt(payload);
    // delay(5000); //para medir consumo
    Serial.printf("Tiempo conexión wifi: %u ms\n", millis() - lastTime);
  }
}

// configuramos puertos e inicalizamos sensores
void inicializacion() {
  lastTime = millis();
  Serial.begin(115200);
  Serial.println("\n-----------------------------------------------");
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_JMP_IN, INPUT_PULLDOWN); // entrada LOW con jumper abierto
  pinMode(PIN_JMP_OUT,OUTPUT);

  //ESP32 - desactiva brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  //ajuste timezone Europe/Madrid
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  // WiFi.mode(WIFI_STA); //modo station por defecto
  //bajamos la frecuencia de la cpu
  bool changeFreq = setCpuFrequencyMhz(80);
  delay(50);
  Serial.flush();
  if(changeFreq) {
    Serial.printf("\nfreq actual   CPU: %u\n", getCpuFrequencyMhz());
  } else Serial.println("error en setCpuFrecuencyMhz()");
  //desactivar wifi
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
  //nombre fichero de log en flash
  flogname = "/";
  flogname.concat(config.ID);
  flogname.concat(".json");
  Serial.printf("nombre fichero log flash: %s\n", flogname.c_str());
  //comprobamos el jumper
  digitalWrite(PIN_JMP_OUT, HIGH);
  jumper_stat = digitalRead(PIN_JMP_IN) == HIGH;
  digitalWrite(PIN_JMP_OUT, LOW);
  Serial.printf("Jumper status: %s\n", jumper_stat ? "CLOSE" : "OPEN");
  httpServer = false; //por defecto apagado
}