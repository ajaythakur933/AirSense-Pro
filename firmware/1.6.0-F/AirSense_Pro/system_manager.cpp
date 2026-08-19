#include "system_manager.h"
#include <Arduino.h>
#include "sensor.h"

unsigned long systemUptimeSeconds()
{
    return millis() / 1000UL;
}

uint32_t systemFreeHeap()
{
    return ESP.getFreeHeap();
}
