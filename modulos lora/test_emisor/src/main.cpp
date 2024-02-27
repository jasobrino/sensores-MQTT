#include <Arduino.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "driver/adc.h"
#include <WiFi.h>
#include "esp_heap_caps.h"
//#include <esp_wifi.h>

#define reyaxRX GPIO_NUM_16 //uart2 esp32
#define reyaxTX GPIO_NUM_17 
#define reyax_RST GPIO_NUM_4 //4 // pin reset del reyax
#define reyaxIRP 115200 //vellocidad en baudios
#define reyax_RECEIVER 1 //modulo receptor
#define MAX_HEAP_MSG 100 //maximo numero de mensajes en cola
// gestion de pila de mensajes
char* str_ptr[MAX_HEAP_MSG];
uint16_t str_ptr_cnt = 0;
uint32_t heap_add(const char* msg);
void heap_get_msg(char* buffer);
//lora
HardwareSerial lora(2);
bool receivedOk = false;
String receivedString = "";
void lora_send(String message);
void receiveCallback();
void hibernate();


void setup() {
  WiFi.disconnect(true);  // Disconnect from the network
  WiFi.mode(WIFI_OFF);    // Switch WiFi off
  //para desactivar el pin de reset de reyax
  gpio_set_pull_mode(reyax_RST, GPIO_PULLUP_ONLY);
  gpio_sleep_set_pull_mode(reyax_RST, GPIO_PULLUP_ONLY);
  //ESP32 - desactiva brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);
  bool changeFreq = setCpuFrequencyMhz(80); //80
  delay(50);
  if(changeFreq) {
    Serial.printf("\nfreq actual   CPU: %u\n", getCpuFrequencyMhz());
  } else Serial.println("error en setCpuFrecuencyMhz()");

  int heap_ini = ESP.getFreeHeap();
  for(int n=1; n <= 500; n++) {
    String frase = "linea de texto numero ";
    frase.concat(n);
    uint32_t resp = heap_add(frase.c_str());
    Serial.printf("%s: %lu\n", frase.c_str(), resp);
  }

  char buffer[240];
  while( str_ptr_cnt > 0) {
    heap_get_msg(buffer);
    Serial.printf("msg: %s   -  heap: %u\n", buffer, ESP.getFreeHeap());
  }
  Serial.printf("heap inicial: %u\n", heap_ini);
  while(true);


  lora.setTxBufferSize(256);
  lora.onReceive(receiveCallback);
  lora.begin(reyaxIRP, SERIAL_8N1, reyaxRX, reyaxTX);
  lora_send("AT");
  // lora_send("AT+SEND=1,98,The switch between receiving mode and sleep mode can be used to achieve the effect of power saving");
  lora_send("AT+SEND=1,239,You should also read all available bytes into a buffer, then write them back out, rather doing an availability check, one byte read, one byte write, and then again in the other direction. This will vastly reduce the time it takes to handle");
  lora_send("AT+MODE=1"); // lora sleep
  lora.flush();

  Serial.println("** deep sleep **");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(8 * 1000000ULL);
  esp_deep_sleep_start();
 // unsigned long last_millis = millis();
/*   lora_send_data("AT+MODE=0");
  lora_send_data("AT+SEND=1,98,The switch between receiving mode and sleep mode can be used to achieve the effect of power saving");
  lora_send_data("AT+MODE=1");

  // gpio_deep_sleep_hold_en();
  Serial.println("esperamos 2 seg");
  Serial.flush();
  delay(2000);
  Serial.println("entrando en deep_sleep(8)");
  esp_deep_sleep_start(); */
}

uint32_t heap_add(const char* msg){
  if( str_ptr_cnt < MAX_HEAP_MSG ) {
    str_ptr[str_ptr_cnt] = (char*) malloc(strlen(msg) + 1);
    strcpy(str_ptr[str_ptr_cnt], msg);
    str_ptr_cnt++;
    return ESP.getFreeHeap();
  }
  return 0;
}

//se devuelve en buffer el mensaje arriba de la pila 
void heap_get_msg(char* buffer) {
  // for(int n=0; n < str_ptr_cnt; n++) printf("[%2d] - %X\n", n, str_ptr[n]);
  /* char* pt = str_ptr[0];
  String msg = "";
  while( *pt != '\0') {
    // Serial.printf("%d - %c\n", *pt, *pt);
    msg.concat(*pt++);
  } */
  strcpy(buffer, str_ptr[0]);
  free(str_ptr[0]);
  for(int n = 0; n < str_ptr_cnt - 1; n++ ) {
    str_ptr[n] = str_ptr[n + 1];
  }
  str_ptr_cnt--;
}

void receiveCallback() {
  size_t available = lora.available();
  receivedString = lora.readString();
  Serial.printf("lora received (%u):%s\n", available, receivedString.c_str());
  receivedOk = true;
}

void lora_send(String message) {
  Serial.printf("lora_send:%s\n", message.c_str());
  lora.println(message.c_str());
  while(!receivedOk){delay(1);}
  receivedOk = false;
}


/* int freeRam() {
heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
info.total_free_bytes;   // total currently free in all non-continues blocks
info.minimum_free_bytes;  // minimum free ever
info.largest_free_block;   // largest continues block to allocate big array
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
}
 */
void hibernate() {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,   ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL,         ESP_PD_OPTION_OFF);
    esp_deep_sleep_start();
}

/* 
String lora_send_data(char const *data) {
  String response;
  Serial.printf("command: %s - ", data);
  lora.write(data);
  lora.write("\r\n");
  uint64_t millis_ant = millis();
  while(!lora.available()) {
    delay(1);
    if( millis() - millis_ant > 3000 ) {
      Serial.println("no hay respuesta en lora.write");
      break;
    }
  }
  while(lora.available() > 0) {
    response = lora.readString();
    Serial.printf("resp: %s\n", response.c_str());
  }
  lora.flush();
  return response;
} */

void loop() {}

