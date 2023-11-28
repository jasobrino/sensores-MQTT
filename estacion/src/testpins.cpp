#include <Arduino.h>

uint8_t pines[] = {32,33,25,26,27,14,12,13};
uint8_t input = 0;
uint8_t pos = 0;

void test() {
    pinMode( input, INPUT);
    for( pos = 0; pos < sizeof(pines); pos++) {
        Serial.print("PIN: ");
        Serial.println(pines[pos]);
        pinMode(pines[pos], OUTPUT);
        digitalWrite(pines[pos], HIGH);
        while( digitalRead(input) == HIGH );
        delay(500);
        while( digitalRead(input) == HIGH );
        digitalWrite(pines[pos], LOW);
        delay(500);
    }    
}