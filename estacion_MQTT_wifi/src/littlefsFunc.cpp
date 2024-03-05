#include <ArduinoJson.h>
#include "global.h"

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

//guardar información en datalog
void saveDatalog(char* payload) {
    if(!LittleFS.begin(false)) {
      Serial.println("fallo al montar LittleFS");
    } else {
      File log = LittleFS.open(flogname.c_str(), FILE_APPEND);
      if(!log) {
        Serial.printf("error apertura archivo %s\n", flogname);
      } else {
        log.printf("%s\n", payload);
        log.close();
      }
    }
}

bool fileExists(String file) {
  if(LittleFS.begin(false)) {
    return LittleFS.exists(file);
  }
  return false;
}
