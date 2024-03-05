
#include "wifiFunc.h"

//objetos wifi
WiFiClient wclient;
PubSubClient mqttClient(wclient);
WiFiManager wifiManager;
WiFiManagerParameter wmpar_sec_sleep("sec_sleep","tiempo sleep (seg)", "240", 4);


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
  //sendMqtt(payload);
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

void sendMqtt(char* payload) {
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

void Wifi_setSleep() {
    WiFi.setSleep(true);
}