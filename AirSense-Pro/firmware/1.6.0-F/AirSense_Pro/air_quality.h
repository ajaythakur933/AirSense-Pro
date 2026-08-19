#ifndef AIR_QUALITY_H
#define AIR_QUALITY_H

#include <Arduino.h>

// =====================================================
// Air Quality Status
// =====================================================

enum AirQualityStatus
{
    AIR_QUALITY_UNKNOWN = 0,
    AIR_QUALITY_GOOD,
    AIR_QUALITY_MODERATE,
    AIR_QUALITY_POOR,
    AIR_QUALITY_VERY_POOR
};

// =====================================================
// Air Quality Result
// =====================================================

struct AirQualityData
{
    AirQualityStatus status;
    float score;
    float rawScore;
    float gasScore;
    float baseline;
    String level;
    String message;
};

// =====================================================
// Initialization
// =====================================================

void initAirQuality();

// =====================================================
// Air Quality Calculation
// =====================================================

AirQualityData calculateAirQuality(
    float gasResistance,
    float humidity,
    float temperature
);

// =====================================================
// Status Helpers
// =====================================================

const char* getAirQualityStatusName(
    AirQualityStatus status
);

#endif