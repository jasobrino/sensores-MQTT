#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "env.h"

#define reyaxRX 16 //uart2 esp32
#define reyaxTX 17 
#define reyax_RST 4 // pin reset del reyax
#define reyaxIRP 57600 //vellocidad en baudios

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire, -1);
HardwareSerial lora(2); //definición módulo en serial1
char message[240];
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
void send_command(char const *command);

void setup() {
  // descativamos el reset
/*   pinMode(reyax_RST, OUTPUT);
  digitalWrite(reyax_RST, HIGH); */

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
  //lora.begin(reyaxIRP, SERIAL_8N1, reyaxRX, reyaxTX);
  lora.begin(reyaxIRP, SERIAL_8N1);
  char str_cpin[20] = {0};
  strcpy(str_cpin,"AT+CPIN=");
  strcat(str_cpin, LORA_CPIN);
  strcat(str_cpin, "\r\n");
  send_command(str_cpin);
  // send_command("AT+CPIN=4B5FF2A9\r\n");
  //send_command( "AT+CPIN?\r\n");
  //send_command( "AT+ADDRESS?\r\n");
  display.print(F("configuracion ok"));
  display.display();
  Serial.println("Entrando en loop...");
}

void loop() {
  while(lora.available() > 0) {
    counter++;
    uint16_t numbytes = lora.readBytesUntil(10, message, sizeof(message));
    // los mensajes terminan con CR+LF, terminamos la cadena eliminando el caracter final
    message[numbytes - 1] = 0;
    Serial.printf("recibido: %06u - %u: [%s]\n", counter,numbytes, message);
    load_msg(message); 
    show_lcd();
  } 
  // char mtrans[50] = {0};
  // sprintf(mtrans, "AT+SEND=100,6,%06u\r\n", counter);
  // reyax.write(mtrans);
  // delay(1000);
}

void send_command(char const *command) {
  Serial.printf("command: %s", command);
  lora.write(command);
  delay(100);
  Serial.flush(true);
  while(lora.available() > 0) {
    Serial.print(lora.readString());
  }
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
  display.printf("rssi:%d snr:%d", msg.rssi, msg.snr);
  display.setCursor(0,20);
  display.printf("cnt:%6d", counter);
  // display.printf("lp:%07u V:%.2f");
  display.display();
}