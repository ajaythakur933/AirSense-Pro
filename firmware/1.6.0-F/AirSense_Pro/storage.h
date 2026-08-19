#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

bool initStorage();

bool isWiFiConfigured();

bool saveWiFiCredentials(const String &ssid, const String &password);

String getWiFiSSID();

String getWiFiPassword();

void clearWiFiCredentials();

#endif