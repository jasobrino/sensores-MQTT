#ifndef sensorSGP30_h
#define sensorSGP30_h

#include <Arduino.h>

bool SGP30_read_values();
void set_baseline_values();
uint32_t getAbsoluteHumidity(float temperature, float humidity);
void spg30_sleep();
void SGP30Calibration();

#endif