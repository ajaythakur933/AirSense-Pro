#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

extern float temperature;
extern float humidity;
extern float pressure;
extern float gasResistance;

extern bool wifiConnected;
extern String ipAddress;
extern int wifiRSSI;

extern int currentPage;

#endif
