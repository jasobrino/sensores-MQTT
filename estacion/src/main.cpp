#include <Arduino.h>
#include "soc/soc.h" 
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_BME280.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "env.h"

#define OLED_RESET -1
#define uS_TO_S_FACTOR 1000000ULL
#define SEC_SLEEP_DEFAULT 60
#define PIN_ADC 36 //se corresponde con SVP
#define BME_ADDR 0x76 //con SDO a Vcc pasa a 0x77
#define SEALEVELPRESSURE_HPA 1013.25 //valor por defecto
#define PIN_LED 12
#define reyaxRX GPIO_NUM_16 //uart2 esp32
#define reyaxTX GPIO_NUM_17 
#define reyax_RST GPIO_NUM_4 //4 // pin reset del reyax
#define reyaxIRP 115200 // 57600 //vellocidad en baudios
#define reyax_RECEIVER 1 //modulo receptor

//definición módulo en serial2
HardwareSerial lora(2);
bool loraReceivedOk = false;
String loraReceivedString = "";

// dispositivos I2C
Adafruit_SGP30 sgp;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire, OLED_RESET);
Adafruit_BME280 bme;

uint32_t lastTime = 0;
uint16_t CO2, TVOC, H2, ETH, eCO2_base, TVOC_base; // SGP30
bool baseline_exists = false;
File f;
float bme_hum, bme_temp, bme_press, bme_alt; //BME280
bool lcd_inst = false, SGP30_inst = false, BME280_inst=false, SGP30_calibration=false;
esp_sleep_wakeup_cause_t wakeup_cause;
RTC_DATA_ATTR uint32_t bootCount = 0; //variable en ram no reseteada por deep sleep

//valores de configuración
struct Config {
  char MQTT_HOST[40];
  uint16_t MQTT_PORT;
  char ID[20];
  uint16_t SEC_SLEEP;
} config;

//objetos wifi
WiFiClient wclient;
PubSubClient mqttClient(wclient);
WiFiManager wifiManager;
WiFiManagerParameter wmpar_sec_sleep("sec_sleep","tiempo sleep (seg)", "240", 4);

// declaración funciones
void show_lcd_sgp30();
bool SGP30_read_values();
float read_vin();
void read_bme();
void bme_sleep();
void spg30_sleep();
void set_baseline_values();
uint32_t getAbsoluteHumidity(float temperature, float humidity);
void get_config();
void save_config();
void Wifi_control();
void wmSaveConfigCallback();
void sendMqtt();
void lora_send(String message);
void lora_receive_callback();
bool lora_send_cpin();
void lora_send_data();

void setup()
{
  lastTime = millis();
  Serial.begin(115200);
  Serial.println("\n-----------------------------------------------");
  Serial.flush();
  // pinMode(PIN_LED, OUTPUT);
  //para desactivar el pin de reset de reyax
  gpio_set_pull_mode(reyax_RST, GPIO_PULLUP_ONLY);
  gpio_sleep_set_pull_mode(reyax_RST, GPIO_PULLUP_ONLY);
  WiFi.disconnect(true);  // Disconnect from the network
  WiFi.mode(WIFI_OFF);    // Switch WiFi off
  //WiFi.setSleep(true); 
  // if(esp_wifi_stop() == ESP_OK) Serial.println("wifi_stop ok");
  // WiFi.mode(WIFI_STA); //modo station por defecto 

  //ESP32 - desactiva brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  //bajamos la frecuencia de la cpu (por defecto 240Mhz)
  bool changeFreq = setCpuFrequencyMhz(80);
  delay(50);
  if(changeFreq) {
    Serial.printf("\nfreq actual   CPU: %u\n", getCpuFrequencyMhz());
  } else Serial.println("error en setCpuFrecuencyMhz()");

  //capturamos la configuración guardada en LittleFS
  get_config(); 
  //configuración del tiempo de sleep
  // iniciando dispositivos y sensores
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setRotation(2);
    display.setTextColor(WHITE);
    lcd_inst = true;
  }

  // if (sgp.begin(&Wire, false)) {
  if (sgp.begin()) {
    SGP30_inst = true;
    Serial.print("SGP30 serial #");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);
    // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
    //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!
  }
  // modulo lora
  lora.setTxBufferSize(256);
  lora.onReceive(lora_receive_callback);
  lora.begin(reyaxIRP, SERIAL_8N1, reyaxRX, reyaxTX);
  if(lcd_inst) {
      display.println("** INIC MOD LORA **");
      display.display();
  }
  lora_send("AT");
  if( loraReceivedString.startsWith("+OK") || loraReceivedString.startsWith("+MODE=0")) {
    Serial.println("modulo lora iniciado correctamente");
    if(lcd_inst) {
      display.println("** MOD LORA OK **");
      display.display();
    }
  } else {
    Serial.println("error en modulo lora");
    if(lcd_inst) {
      display.println("** MOD LORA ERROR **");
      display.display();
    }
    while(true) { delay(10); }
  }
  lora_send("AT+MODE=1"); // modo sleep de lora
  
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
    // ponemos el BME en sleep
    bme_sleep();
  }

  // causa wakeup
  wakeup_cause = esp_sleep_get_wakeup_cause();
  switch(wakeup_cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("wakeup causado por TIMER");
      break;
    //al arrancar o hacer reset se entra en default
    default: 
      Serial.println("wakeup no causado por deep sleep");
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setRotation(2);
      display.clearDisplay();
      display.setCursor(0,0);
      display.printf("%s RESTART\n", config.ID);

      if(SGP30_inst) {
        //ahora asignamos los valores de baseline guardados
        set_baseline_values();
        //no se han podido abrir los archivos de baseline
        if(SGP30_calibration) {
          //ahora debemos calcular los valores de baseline del SGP30
          // los primeros 10 minutos en el exterior, el proceso tarda 12 horas
          Serial.println("calculando baseline (12h)");
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
    spg30_sleep();
  }   
  if(BME280_inst) {
    Serial.printf("BME280: temp:%.2fC hum:%.2f%% press:%.2f hPa Alt:%.2f m\n",
                  bme_temp, bme_hum, bme_press, bme_alt);
  }
  bootCount++;
  if(lcd_inst) show_lcd_sgp30();
  Serial.printf("Tiempo lectura sensores: %u ms\n", millis() - lastTime);
  lastTime = millis();

  // if(SGP30_inst) spg30_sleep(); //apagamos el SGP30 (soft reset)
  // if(BME280_inst) bme_sleep(); //reducimos el consumo del BME280
  //activamos el wifi y enviamos los datos por mqtt
  // if(esp_wifi_start() == ESP_OK) Serial.println("wifi restablecido");
  //Wifi_control();
  Serial.printf("Tiempo conexión wifi: %u ms\n", millis() - lastTime);
  lora_send("AT+MODE=0"); // modo transceiver lora
  lora_send_cpin();
  lora_send_data(); //no usamos wifi
  lora_send("AT+MODE=1"); // modo sleep de lora
  lora.flush();
  if(lcd_inst) display.ssd1306_command(SSD1306_DISPLAYOFF); //apagar lcd
  // delay(5000); //para medir consumo
  // pasamos a modo deep sleep
  Serial.println("**** deep_sleep ****\n");
  Serial.flush();
  delay(20);
  esp_sleep_enable_timer_wakeup(config.SEC_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
} 

void loop() {}

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

//activar la conexión wifi y proceder a enviar los datos
void Wifi_control() {
  WiFi.mode(WIFI_STA); //modo station por defecto
  //wifiManager.resetSettings(); //reinicia la config wifi
  //Realizamos la conexión wifi mediante wifiManager
  //si no lo consigue, creará un AP en 192.168.4.1 para su configuracion
  wifiManager.setConfigPortalTimeout(180); //tiempo espera AP en seg
  wifiManager.setConnectTimeout(60); //tiempo max conexion a router
  // wifiManager.resetSettings(); //borra las credenciales wifi
  //añadimos campo para captura de tiempo
  wifiManager.addParameter(&wmpar_sec_sleep);
  wifiManager.setSaveConfigCallback(wmSaveConfigCallback);
  //publicamos el SSID con el nombre del dispositivo
  Serial.printf("wifiManager(%s) conectando wifi", config.ID);
  wifiManager.autoConnect(config.ID,"superraton");
  //wifiManager.startConfigPortal("wifiManager");
  // enviamos los datos por MQTT
  sendMqtt();
}

//funcion callback despues de entrar en WifiManager
void wmSaveConfigCallback() {
  const char* par_sec_sleep = wmpar_sec_sleep.getValue();
  Serial.printf("par_sec_sleep: %s\n", par_sec_sleep);
  config.SEC_SLEEP = atoi(par_sec_sleep);
  Serial.println("valores actualizados:");
  Serial.printf("ID:%s, host:%s, port:%d, sleep: %d\n",config.ID, config.MQTT_HOST, config.MQTT_PORT, config.SEC_SLEEP);
  save_config(); //guardamos la configuracion en config.json
}


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
//desactivar el SGP30
void spg30_sleep() {
  Wire.beginTransmission(0x00); // General Call mode -> writing to address 0 creates a 0x00 first byte
  Wire.write(0x06);
  Wire.endTransmission();
}
//cálculo de la humedad absoluta - necesario para el SGP30
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
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
// lectura estado bateria por el ADC
float read_vin() {
  #define FACT_CORR 1.73
  uint16_t v = analogRead(PIN_ADC);
  float ADCv = (((v * 3.3) / 4095) + 0.1) * FACT_CORR;
  return ADCv;
}
// envio de comandos a modulo lora
void lora_receive_callback() {
  size_t available = lora.available();
  loraReceivedString = lora.readString();
  Serial.printf("lora received (%u):%s\n", available, loraReceivedString.c_str());
  loraReceivedOk = true;
}

void lora_send(String message) {
  Serial.printf("lora_send:%s\n", message.c_str());
  lora.println(message.c_str());
  while(!loraReceivedOk){delay(1);}
  loraReceivedOk = false;
}
// enviar codigo cpin a modulo lora
bool lora_send_cpin() {
  char str_cpin[20] = {0};
  strcpy(str_cpin,"AT+CPIN=");
  strcat(str_cpin, LORA_CPIN);
  lora_send(str_cpin);
  if ( !loraReceivedString.startsWith("+") ) {
    Serial.println("lora_send_command: error");
    return false;
  } 
  return true;
}
// envio datos sensores
void lora_send_data() {
  char buffer[100];

  String lora_msg = "AT+SEND=";
  snprintf(buffer, sizeof(buffer),"%u,", reyax_RECEIVER);
  lora_msg.concat(buffer);
  //añadimos datos de los sensores
  snprintf(buffer, sizeof(buffer), "ID%s",config.ID);
  String payload = buffer;
  if(BME280_inst) {
    char str_hum[10], str_temp[10], str_press[10];
    dtostrf(bme_hum, 6, 2, str_hum);    
    dtostrf(bme_temp, 6, 2, str_temp);
    dtostrf(bme_press, 8, 2, str_press);
    snprintf(buffer,  sizeof(buffer), ",TP%s,HM%s,PR%s", str_temp, str_hum, str_press);
    payload.concat(buffer);
  }
  if(SGP30_inst) {
    snprintf(buffer, sizeof(buffer), ",CO2%u,TV%u", CO2, TVOC);
    payload.concat(buffer);
  }
  //datos falsos de SGP30
  // snprintf(buffer, sizeof(buffer), ",CO2%u,TV%u", 2133, 23566);
  // payload.concat(buffer);

  char str_vbat[10];
  dtostrf(read_vin(), 4, 2, str_vbat);
  snprintf(buffer,  sizeof(buffer), ",VB%s", str_vbat);
  payload.concat(buffer);
  payload.replace(" ",""); //quitamos los espacios
  //payload.replace(",",":");
  uint16_t payload_length = payload.length();
  snprintf(buffer, sizeof(buffer), "%u,", payload_length);
  lora_msg.concat(buffer);
  lora_msg.concat(payload);
  lora_msg.replace(" ","");
  lora_send(lora_msg.c_str());
}

// envio de datos al broker MQTT
void sendMqtt() {
  char payload[200];
  char buffer[100];

  snprintf(payload, sizeof(payload), "{ \"ID\": \"%s\"", config.ID);
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
  snprintf(buffer,  sizeof(buffer), ", \"VBAT\": %s, \"RSSI\": %d }", str_vbat, WiFi.RSSI());
  strcat(payload, buffer);
  Serial.printf("payload:%s\n", payload);
  //enviamos los datos al broker mqtt
  mqttClient.setServer(config.MQTT_HOST, config.MQTT_PORT);
  if(mqttClient.connect(config.ID)) {
    Serial.println("conectado a MQTT");
    mqttClient.publish("sensor/values", payload);
    mqttClient.disconnect();
    mqttClient.flush();
    // esperamos hasta que la conexión se cierre completamente
    while(mqttClient.state() != -1) {
      delay(10);
    }
  } else {
    Serial.print("MQTT error de conexion:");
    Serial.println(mqttClient.state());
  }
}

//leemos los valores del archivo de config.json
void get_config() {
  //montamos el sistema de archivos
  const char* fconf = "/config.json";
  if(!LittleFS.begin(false)) {
    Serial.println("fallo al montar LittleFS");
  } else {
    if( LittleFS.exists(fconf)) {
      File f = LittleFS.open(fconf);
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, f);
      if (error) {
        Serial.println(F("JsonDocument: error al leer el archivo"));
      } else {
        //leemos las variables de configuración
        strlcpy(config.MQTT_HOST, doc["MQTT_HOST"] | "linux-mqtt", sizeof(config.MQTT_HOST));
        strlcpy(config.ID, doc["ID"] | "NO_ID", sizeof(config.ID));
        config.MQTT_PORT = doc["MQTT_PORT"] | 1883;
        config.SEC_SLEEP = doc["SEC_SLEEP"] | SEC_SLEEP_DEFAULT;
        Serial.println("leido fichero de configuración:");
        Serial.printf("ID:%s, host:%s, port:%d, sleep: %d\n",config.ID, config.MQTT_HOST, config.MQTT_PORT, config.SEC_SLEEP);
      }
      f.close();
    } else {
      Serial.printf("no se ha encontrado el archivo %s\n", fconf);
    } 
  }
}

//guardamos los valores en config.json
void save_config() {
  //montamos el sistema de archivos
  const char* fconf = "/config.json";
  if(!LittleFS.begin(false)) {
    Serial.println("fallo al montar LittleFS");
  } else {
    DynamicJsonDocument doc(500);
    doc["ID"] = config.ID;
    doc["MQTT_HOST"] = config.MQTT_HOST;
    doc["MQTT_PORT"] = config.MQTT_PORT;
    doc["SEC_SLEEP"] = config.SEC_SLEEP;
    Serial.println("datos JSON serializados:");
    serializeJsonPretty(doc, Serial);
    //creamos el fichero config.json
    File conf = LittleFS.open(fconf, "w", true);
    serializeJsonPretty(doc, conf);
    conf.close();
  }
}