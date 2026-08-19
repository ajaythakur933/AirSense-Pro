#ifndef SENSOR_H
#define SENSOR_H

#include <Adafruit_BME680.h>

bool initSensor();
bool updateSensor();

uint32_t sensorGetReadCount();
uint32_t sensorGetFailureCount();
uint32_t sensorGetRecoveryCount();
uint32_t sensorGetStaleCount();
uint32_t sensorGetRecoveryAttemptCount();
uint32_t sensorGetOfflineEventCount();
uint8_t sensorGetRecoveryAttemptsSinceFailure();
bool sensorIsOffline();
bool sensorIsHealthy();

#endif