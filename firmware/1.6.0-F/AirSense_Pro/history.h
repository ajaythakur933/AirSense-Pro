#ifndef HISTORY_H
#define HISTORY_H

#include <Arduino.h>
#include "air_quality.h"

struct HistorySample
{
    uint32_t uptimeSeconds;
    float temperature;
    float humidity;
    float pressure;
    float gasResistance;
    float airQualityScore;
};

bool initHistory();
void historyLoop();
void historyRecord(float temperature,
                   float humidity,
                   float pressure,
                   float gasResistance,
                   float airQualityScore);

uint16_t historyGetCount();
uint16_t historyGetCapacity();
uint16_t historyGetWindowCount(uint32_t windowSeconds);
const HistorySample* historyGetSamples();
bool historyGetSample(uint16_t index, HistorySample &sample);
uint32_t historyGetSampleSequence(uint16_t index);
uint32_t historyGetSampleIntervalSeconds();
uint32_t historyGetLastSampleUptime();
bool historyIsReady();

#endif
