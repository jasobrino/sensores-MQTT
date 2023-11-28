#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define WIFI_MAX_DELAY 8000 //max tiempo espera wifi en ms
#define RESTART_TIME 60000L //tiempo total entre reinicios en ms
#define uS_TO_S_FACTOR 1000000 
#define SEALEVELPRESSURE_HPA 1013.25 //valor por defecto
#define BME_ADDR 0x76 //con SDO a Vcc pasa a 0x77
#define DEEPSLEEP_TIME 6000 //tiempo hasta el reinicio en ms
#define PIN_ADC 36 //se corresponde con SVP

const char* ssid = "MQTT_ROUTER";
const char* pass = "superRat0n";

// sensor
float bme_hum, bme_temp, bme_press, bme_alt;
bool bme_pressent;
// dispositivos I2C
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);
Adafruit_BME280 bme;

//valores de configuración
struct Config {
  char MQTT_HOST[40];
  uint16_t MQTT_PORT;
  char ID[20];
} config;

//objetos wifi
WiFiClient wclient;
PubSubClient mqttClient(wclient);
WiFiManager wifiManager;

RTC_DATA_ATTR int bootCount = 0; //variable en ram no reseteada por deep sleep
unsigned long t_start=0, t_boot=0;
unsigned long t_remain=0;

void Wifi_conn();
void sendMqtt();
void wakeup_ctrl();
void show_lcd_data(String s);
void read_bme();
void bme_sleep();
void get_config();
float read_vin();

void setup() {
  ++bootCount;
  t_boot = millis();
  Wire.begin();
  Serial.begin(115200);
  Serial.println("\n-------------------------");
  Serial.println("inicio rutina");
  get_config();

  //sensor BME280
  if(!bme.begin(BME_ADDR)) {
    Serial.println("no se ha encontrado BME280");
    bme_pressent = false;
  } else {
    //leer datos del sensor
    read_bme();
    bme_pressent = true;
  }
  Serial.printf("BME280: temp:%.2fC hum:%.2f%% press:%.2f hPa Alt:%.2f m\n",
          bme_temp, bme_hum, bme_press, bme_alt);
  Serial.printf("ADC:%u - volt: %.2f\n", analogRead(PIN_ADC), read_vin());
  // analisis motivo reinicio
  wakeup_ctrl();
  //ESP32 - desactiva brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  t_start = millis(); //iniciamos el conteo de tiempo
  WiFi.mode(WIFI_STA); //modo station por defecto
  //wifiManager.resetSettings(); //reinicia la config wifi
  //Realizamos la conexión wifi mediante wifiManager
  //si no lo consigue, creará un AP en 192.168.4.1 para su configuracion
  wifiManager.setConfigPortalTimeout(180); //tiempo espera AP en seg
  wifiManager.setConnectTimeout(60); //tiempo max conexion a router
  //wifiManager.resetSettings(); //borra las credenciales wifi
  wifiManager.autoConnect("miESP32","superraton");
  //wifiManager.startConfigPortal("wifiManager");
  unsigned long t_total = millis() - t_start;
  Serial.printf("Tiempo acceso wifi: %lu\n", t_total);
  t_start = millis();
  // enviamos los datos por MQTT
  sendMqtt();
  unsigned long t_envio = millis() - t_start;
  Serial.printf("Tiempo envio datos : %lu\n", t_envio);
  Serial.printf("Tiempo total activo: %lu\n", t_envio + t_total);

  delay(100);
  Serial.println("entrando en deep-sleep");
  //calculamos el tiempo que falta para RESTART_TIME
  ulong t_transcurrido = millis() - t_boot;
  if( t_transcurrido > RESTART_TIME ) {
    t_remain = RESTART_TIME;
  } else {
    t_remain = RESTART_TIME - t_transcurrido;
  }
  Serial.printf("tiempo espera: %lu\n", t_remain);
  // if(t_remain < 0) {
  //   Serial.printf("t_remain negativo: %d - ajustando valor a %d\n", t_remain, RESTART_TIME);
  //   t_remain = RESTART_TIME; //no puede ser negativo
  // }
  Serial.flush();
  display.ssd1306_command(SSD1306_DISPLAYOFF); //apagar lcd
  //pasamos a deepSleep
  if(bme_pressent) bme_sleep(); //reducimos el consumo del sensor
  ESP.deepSleep(t_remain * 1000); //deep-sleep 
}

void loop() {}

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
        Serial.println("leido fichero de configuración:");
        Serial.printf("host:%s, port:%d, ID:%s\n", config.MQTT_HOST, config.MQTT_PORT, config.ID);
      }
      f.close();
    } else {
      Serial.printf("no se ha encontrado el archivo %s\n", fconf);
    } 
  }
}

void sendMqtt() {
  char payload[200];
  snprintf(payload, sizeof(payload), 
      "{ \"ID\": \"%s\", \"temp\": %.2f, \"humedad\": %.2f, \"presion\": %.2f, "
      "\"VBAT\": %.2f, \"RSSI\": %d }", config.ID, bme_temp, bme_hum, bme_press, read_vin(), WiFi.RSSI());
  Serial.print("payload: ");
  Serial.println(payload);
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

void read_bme() {
  bme_temp = bme.readTemperature();
  bme_hum = bme.readHumidity();
  bme_press = bme.readPressure() / 100.0F;
  bme_alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
}

void bme_sleep() {
  Wire.beginTransmission(BME_ADDR);
  Wire.write((uint8_t) 0xF4);
  Wire.write((uint8_t) 0x00);
  Wire.endTransmission();
}

float read_vin() {
  #define FACT_CORR 1.73
  uint16_t v = analogRead(PIN_ADC);
  float ADCv = (((v * 3.3) / 4095) + 0.1) * FACT_CORR;
  return ADCv;
}

void wakeup_ctrl() {
  esp_sleep_wakeup_cause_t wake_up;
  //read_bme();

  wake_up = esp_sleep_get_wakeup_cause();
  switch(wake_up) {
    //detectado deep sleep
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("wakeup TIMER"); 
      char mesg[20];
      sprintf(mesg, "V:%.2f loop:%d", read_vin(), bootCount);
      if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        show_lcd_data(String(mesg));
        delay(5000);
      }
      break;
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("wakeup por señal externa RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("wakeup por señal externa RTC_CNTL"); break;
    //entra con un reset
    default : Serial.printf("Wakeup no causado por Deep Sleep: %d\n",wake_up); 
      if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        //pantalla conectada
        show_lcd_data("RESET: V:" + String(read_vin()));
        delay(5000);
      } else {
        Serial.println("pantalla no detectada");
      }
  }
}

void show_lcd_data(String s) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(5,0);
  display.printf("%3.2fC %2.2f%%", bme_temp, bme_hum);
  display.setCursor(5,10);
  display.printf("%.2f hPa %.2f m", bme_press, bme_alt);
  display.setCursor(5,20);
  display.print(s);
  display.display();
}

void Wifi_conn() {
  // asignacion de IP fija
  /* IPAddress subnet(255,255,255,0);
  // IPAddress staticIP(10,3,141,50);
  // IPAddress gateway(10,3,141,1);
  IPAddress staticIP(192,168,2,220);
  IPAddress gateway(192,168,2,1);
  WiFi.config(staticIP, gateway, subnet);
  */
  WiFi.persistent(false); // para no regrabar los valores en la flash
  WiFi.begin(ssid, pass);
  WiFi.printDiag(Serial);
  unsigned long t_ini = millis();
  Serial.print("conectando wifi\t");

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print('.'); 
    if( millis() - t_ini > WIFI_MAX_DELAY ) {
      Serial.println(" ESP.restart");
      ESP.restart();
    }
  }
  Serial.print("\nConectado a: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi.RSSI: ");
  Serial.println(WiFi.RSSI());
}