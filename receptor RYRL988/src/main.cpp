#include <Arduino.h>
#include "soc/soc.h" 
#include "soc/rtc_cntl_reg.h"
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "env.h"

#define reyaxRX GPIO_NUM_16 //uart2 esp32
#define reyaxTX GPIO_NUM_17 
#define reyax_RST GPIO_NUM_4 //4 // pin reset del reyax
#define reyaxIRP 115200  //57600 //vellocidad en baudios

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire, -1);
HardwareSerial lora(2); //definición módulo en serial2
char message[260];
struct st_msg {
  uint16_t tr_addr;
  uint16_t payload_len;
  char payload[240];
  int16_t rssi;
  int16_t snr;
} msg;

uint16_t counter = 0;
char literal[]="+RCV=100,4,0123,-5,12";
void show_lcd();
void load_msg(char *ms);
String lora_send_command(char const *command);
bool lora_send_cpin();

void setup() {
  // descativamos el reset
/*   pinMode(reyax_RST, OUTPUT);
  digitalWrite(reyax_RST, HIGH); */
  //ESP32 - desactiva brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  //para desactivar el pin de reset de reyax
  gpio_set_pull_mode(reyax_RST, GPIO_PULLUP_ONLY);

  Serial.begin(115200);
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
  lora.begin(reyaxIRP, SERIAL_8N1, reyaxRX, reyaxTX);
  // lora.begin(reyaxIRP, SERIAL_8N1);
  if(lora_send_cpin()) {
    Serial.println("CPIN enviado correctamente a modulo lora.");
  }
 
  // send_command("AT+CPIN=11223344\r\n");
  // send_command( "AT+CPIN?\r\n");
  // send_command( "AT+ADDRESS?\r\n");
  Serial.println("Entrando en loop...");
}

void loop() {
  while(lora.available() > 0) {
    counter++;
    uint16_t numbytes = lora.readBytesUntil(10, message, sizeof(message));
    //uint16_t numbytes = lora.readBytes(message, 260);
    // los mensajes terminan con CR+LF, terminamos la cadena eliminando el caracter final
    message[numbytes - 1] = 0;
    Serial.printf("recibido: %06u - %u: [%s]\n", counter,numbytes, message);
    // comprobamos que el mensaje recibido no es un error
    if( strncmp("+RCV", message, 4) == 0) {
      load_msg(message); 
    } else {
      strcpy( msg.payload, message);
    }
    show_lcd();
  } 
}

// envio de comandos a modulo lora
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