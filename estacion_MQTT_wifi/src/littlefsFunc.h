#ifndef littlefsFunc_h
#define littlefsFunc_h

#include <Arduino.h>
#include "global.h"

void get_config();
void save_config();
void saveDatalog(char* payload);
bool fileExists(String file);

#endif