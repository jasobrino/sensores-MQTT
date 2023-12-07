#include <Arduino.h>
#include "soc/soc.h" 
#include "soc/rtc_cntl_reg.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "env.h"

#define uS_TO_S_FACTOR 1000000ULL
#define reyaxRX GPIO_NUM_16 //uart2 esp32
#define reyaxTX GPIO_NUM_17 
#define reyax_RST GPIO_NUM_4 // pin reset del reyax
#define reyaxIRP 115200  //57600 //vellocidad en baudios

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire, -1);
//definición módulo en serial2
HardwareSerial lora(2);
bool loraReceivedOk = false;
String loraReceivedString = "";
String json_payload;

char message[260];
struct st_msg {
  uint16_t tr_addr;
  uint16_t payload_len;
  char payload[240];
  int16_t rssi;
  int16_t snr;
} msg;

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

// contador recepcion
uint16_t counter = 0;

// declaración funciones
void show_lcd();
void Wifi_control();
void get_config();
// void save_config();
void sendMqtt();
void load_msg(char *ms);
void lora_send(String message);
void lora_receive_callback();
bool lora_send_cpin();
void lora_send_data();

void setup() {
  //ESP32 - desactiva brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  //para desactivar el pin de reset de reyax
  gpio_set_pull_mode(reyax_RST, GPIO_PULLUP_ONLY);
  gpio_sleep_set_pull_mode(reyax_RST, GPIO_PULLUP_ONLY);
  Serial.begin(115200);
  //capturamos la configuración guardada en LittleFS
  get_config(); 
  // load_msg(literal);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("error en display.begin()"));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setRotation(2);
    display.setTextColor(WHITE);
    display.println(F("enviando comandos AT"));
    display.display();
  }
  Serial.println("iniciado uart2: ");
  lora.setRxBufferSize(256);
  lora.onReceive(lora_receive_callback);
  lora.begin(reyaxIRP, SERIAL_8N1, reyaxRX, reyaxTX);
  if(lora_send_cpin()) {
    Serial.println("CPIN enviado correctamente a modulo lora.");
  }
  uint64_t lastTime = millis();
  Wifi_control();
  Serial.printf("Tiempo conexión wifi: %u ms\n", millis() - lastTime);
  Serial.println("Entrando en loop...");
}

void loop() {
  // nuevo mensaje recibido
  if(loraReceivedOk) {
    counter++;
    loraReceivedOk = false;
    uint16_t numbytes = loraReceivedString.length();
    strncpy(message, loraReceivedString.c_str(), sizeof(message));
    Serial.printf("recibido: %06u - %u: [%s]\n", counter, numbytes, message);
    // comprobamos que el mensaje recibido no es un error
    if( loraReceivedString.startsWith("+RCV") ) {
      load_msg(message); 
      //envio de datos por MQTT
      sendMqtt();
    } else {
      strcpy( msg.payload,message);
    }
    show_lcd();
  }
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

// capturamos los valores del string
// +RCV=100,4,0123,-5,12
void load_msg(char *ms) {
  char * strPos;
  strPos = strtok(ms, "="); //saltamos el signo =
  strPos = strtok(NULL, ",");
  msg.tr_addr = atoi(strPos);
  strPos = strtok(NULL, ",");
  msg.payload_len = atoi(strPos);
  strPos = strtok(NULL, ",");
  strcpy(msg.payload, strPos);
  strPos = strtok(NULL, ",");
  msg.rssi = atoi(strPos);
  strPos = strtok(NULL, ",");
  msg.snr = atoi(strPos);
  Serial.printf("addr: %u, len:%u, msg:%s, rssi: %d, snr: %d\n",
          msg.tr_addr,msg.payload_len,msg.payload,msg.rssi,msg.snr);
  //analizamos el payload
  json_payload = "{\"ID\":";
  json_payload.concat(msg.tr_addr); //identificador address mod lora = estacion
  json_payload.concat(",");
  String str_payload = String(msg.payload);
  str_payload.replace(":",",");
  str_payload.replace("TP","\"temp\":");
  str_payload.replace("HM","\"humedad\":");
  str_payload.replace("PR","\"presion\":");
  str_payload.replace("CO","\"co2\":");
  str_payload.replace("TV","\"tvoc\":");
  str_payload.replace("VB","\"vbat\":");
  json_payload.concat(str_payload);
  json_payload.concat(",\"rssi\":");
  json_payload.concat(msg.rssi);
  json_payload.concat("}");
  Serial.println(json_payload);
}

//mostrar información en el lcd
void show_lcd() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.printf("%s",msg.payload);
  display.setCursor(0,10);
  //display.printf("rssi:%d snr:%d", msg.rssi, msg.snr);
  display.setCursor(0,20);
  display.printf("cnt:%6d", counter);
  // display.printf("lp:%07u V:%.2f");
  display.display();
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
  // wifiManager.addParameter(&wmpar_sec_sleep);
  // wifiManager.setSaveConfigCallback(wmSaveConfigCallback);
  //publicamos el SSID con el nombre del dispositivo
  Serial.printf("wifiManager(%s) conectando wifi", config.ID);
  wifiManager.autoConnect(config.ID,"superraton");
  //wifiManager.startConfigPortal("wifiManager");
}

// envio de datos al broker MQTT
void sendMqtt() {
  mqttClient.setServer(config.MQTT_HOST, config.MQTT_PORT);
  if(mqttClient.connect(config.ID)) {
    Serial.println("conectado a MQTT");
    mqttClient.publish("sensor/values", json_payload.c_str());
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
        Serial.println("leido fichero de configuración:");
        Serial.printf("ID:%s, host:%s, port:%d, sleep: %d\n",config.ID, config.MQTT_HOST, config.MQTT_PORT, config.SEC_SLEEP);
      }
      f.close();
    } else {
      Serial.printf("no se ha encontrado el archivo %s\n", fconf);
    } 
  }
}

/* // envio de comandos a modulo lora
String lora_send_command(char const *command) {
  String loraResponse;
  if(lora.available() > 0) {
    Serial.printf("datos en buffer recepción: %s\n", lora.readString());
  }
  Serial.printf("command: %s\n", command);
  size_t nbytes = lora.write(command);
  lora.write("\r\n");
  uint64_t lastMillis = millis();
  while(!lora.available()) {
    delay(1);
    if(millis() - lastMillis > 10000) {
      Serial.println("lora_send_command: error en modulo lora");
      return String("ERR");
    }
  }
  while(lora.available() > 0) {
    loraResponse = lora.readString();
    Serial.printf("resp: %s\n", loraResponse.c_str());
  }
  lora.flush();
  Serial.printf("bytes enviados: %u\n", nbytes);
  return loraResponse;
}

bool lora_send_cpin() {
  char str_cpin[20] = {0};
  strcpy(str_cpin,"AT+CPIN=");
  strcat(str_cpin, LORA_CPIN);
  String loraResponse = lora_send_command(str_cpin);
  if ( !loraResponse.startsWith("+OK") ) {
    Serial.println("error en comando lora");
    display.print(F("** ERROR MOD LORA **"));
    return false;
  } 
  display.print(F("configuracion ok"));
  display.display();
  return true;
}
 */