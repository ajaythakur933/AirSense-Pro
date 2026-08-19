#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

bool initWiFi();
bool connectToSavedWiFi();

void updateWiFi();

bool isWiFiConnected();

// V1.5.0-B diagnostics
uint32_t wifiGetDisconnectCount();
uint32_t wifiGetReconnectAttemptCount();
uint32_t wifiGetReconnectSuccessCount();
uint32_t wifiGetReconnectFailureCount();

// Returns true once when a previously connected Wi-Fi link
// has successfully reconnected. The event is consumed by
// the caller.
bool consumeWiFiReconnectEvent();

#endif
