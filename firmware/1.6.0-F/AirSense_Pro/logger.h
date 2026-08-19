#pragma once

#include <Arduino.h>
#include <WiFi.h>

#ifndef AIRSENSE_TCP_LOG_PORT
#define AIRSENSE_TCP_LOG_PORT 2323
#endif

void loggerBegin();
void loggerLoop();

void loggerPrint(const String &msg);
void loggerPrintln(const String &msg);
void loggerPrintln(const char *msg);

bool loggerClientConnected();

uint32_t loggerGetConnectionCount();
uint32_t loggerGetDisconnectCount();

// Convenience overloads used by existing Serial-style firmware diagnostics.
template <typename T>
inline void loggerPrint(T value) {
  loggerPrint(String(value));
}

inline void loggerPrint(float value, int digits) {
  loggerPrint(String(value, digits));
}

inline void loggerPrint(double value, int digits) {
  loggerPrint(String(value, digits));
}

template <typename T>
inline void loggerPrintln(T value) {
  loggerPrintln(String(value));
}
