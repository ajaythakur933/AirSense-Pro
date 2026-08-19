#include "air_quality.h"
#include <Preferences.h>

// =====================================================
// AirSense Pro - Relative Air Quality Calibration
// =====================================================
//
// IMPORTANT:
// BME680 gas resistance is a relative VOC/gas indicator.
// It is NOT a direct CO2/PM2.5/AQI measurement.
//
// This version replaces the fixed absolute gas thresholds
// with a device-specific clean-air baseline.
//
// Calibration flow:
//   1. Existing saved baseline is loaded from NVS.
//   2. If no baseline exists, the sensor warms up.
//   3. A calibration window collects valid gas readings.
//   4. The average gas resistance becomes the baseline.
//   5. The baseline is stored in ESP32 NVS.
//   6. Future scores are calculated relative to that baseline.
//
// The first calibration should be performed in reasonably
// clean, normal indoor air and without intentionally exposing
// the sensor to perfume, smoke, alcohol vapour, cooking fumes,
// or other strong VOC sources.
//
// =====================================================

// Humidity comfort range
#define HUMIDITY_MIN        30.0f
#define HUMIDITY_MAX        70.0f

// Calibration timing.
// Sensor readings currently arrive approximately once per second.
#define AQ_CALIBRATION_WARMUP_MS       120000UL   // 2 minutes
#define AQ_CALIBRATION_SAMPLES         120U       // ~2 minutes
#define AQ_CALIBRATION_MIN_GAS         0.1f

// NVS namespace/key
#define AQ_PREF_NAMESPACE              "airquality"
#define AQ_PREF_BASELINE_KEY           "gasBase"

// Baseline validity range in kOhm.
// BME680 values outside this range are rejected for baseline use.
#define AQ_BASELINE_MIN                0.1f
#define AQ_BASELINE_MAX                10000.0f

// =====================================================
// Internal State
// =====================================================

static Preferences aqPreferences;

static bool calibrationActive = false;
static bool calibrationComplete = false;
static bool baselineLoaded = false;

static unsigned long calibrationStartTime = 0;

static uint16_t calibrationSampleCount = 0;
static double calibrationGasSum = 0.0;

static float gasBaseline = 0.0f;

// v1.6.0-E air-quality stability filter.
// A short median window rejects isolated gas-resistance spikes,
// while an EMA keeps the displayed score readable without hiding
// genuine sustained environmental changes.
static float gasFilterSamples[5] = {0, 0, 0, 0, 0};
static uint8_t gasFilterCount = 0;
static uint8_t gasFilterIndex = 0;
static float smoothedScore = NAN;
static float lastInputGas = NAN;
static float lastInputHumidity = NAN;
static float lastInputTemperature = NAN;
static float lastRawScore = NAN;
static float lastGasScore = NAN;

static const float AQ_SCORE_EMA_ALPHA = 0.25f;

static void resetAirQualityFilter()
{
    for (uint8_t i = 0; i < 5; ++i)
        gasFilterSamples[i] = 0.0f;

    gasFilterCount = 0;
    gasFilterIndex = 0;
    smoothedScore = NAN;
    lastInputGas = NAN;
    lastInputHumidity = NAN;
    lastInputTemperature = NAN;
    lastRawScore = NAN;
    lastGasScore = NAN;
}

static bool airQualityInputsChanged(float gas, float humidity, float temperature)
{
    return !isfinite(lastInputGas) ||
           fabsf(gas - lastInputGas) > 0.0001f ||
           fabsf(humidity - lastInputHumidity) > 0.0001f ||
           fabsf(temperature - lastInputTemperature) > 0.0001f;
}

static float medianGasResistance(float gasResistance)
{
    gasFilterSamples[gasFilterIndex] = gasResistance;
    gasFilterIndex = (uint8_t)((gasFilterIndex + 1) % 5);
    if (gasFilterCount < 5)
        gasFilterCount++;

    float sorted[5];
    for (uint8_t i = 0; i < gasFilterCount; ++i)
        sorted[i] = gasFilterSamples[i];

    for (uint8_t i = 1; i < gasFilterCount; ++i)
    {
        float key = sorted[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && sorted[j] > key)
        {
            sorted[j + 1] = sorted[j];
            --j;
        }
        sorted[j + 1] = key;
    }

    if (gasFilterCount % 2 == 1)
        return sorted[gasFilterCount / 2];

    return (sorted[(gasFilterCount / 2) - 1] + sorted[gasFilterCount / 2]) * 0.5f;
}

// =====================================================
// Save Baseline
// =====================================================

static bool saveBaseline(float baseline)
{
    if (!isfinite(baseline) ||
        baseline < AQ_BASELINE_MIN ||
        baseline > AQ_BASELINE_MAX)
    {
        return false;
    }

    if (!aqPreferences.begin(
            AQ_PREF_NAMESPACE,
            false))
    {
        return false;
    }

    size_t written =
        aqPreferences.putFloat(
            AQ_PREF_BASELINE_KEY,
            baseline
        );

    aqPreferences.end();

    return written == sizeof(float);
}

// =====================================================
// Load Baseline
// =====================================================

static bool loadBaseline()
{
    if (!aqPreferences.begin(
            AQ_PREF_NAMESPACE,
            true))
    {
        return false;
    }

    float storedBaseline =
        aqPreferences.getFloat(
            AQ_PREF_BASELINE_KEY,
            0.0f
        );

    aqPreferences.end();

    if (!isfinite(storedBaseline) ||
        storedBaseline < AQ_BASELINE_MIN ||
        storedBaseline > AQ_BASELINE_MAX)
    {
        return false;
    }

    gasBaseline = storedBaseline;
    baselineLoaded = true;
    calibrationComplete = true;

    return true;
}

// =====================================================
// Start Calibration
// =====================================================

static void startCalibration()
{
    calibrationActive = true;
    calibrationComplete = false;
    baselineLoaded = false;

    calibrationStartTime = millis();

    calibrationSampleCount = 0;
    calibrationGasSum = 0.0;

    gasBaseline = 0.0f;

    Serial.println();
    Serial.println("==============================");
    Serial.println("Air Quality Calibration");
    Serial.println("==============================");
    Serial.println("No saved gas baseline found.");
    Serial.println("Sensor warm-up started.");
    Serial.println("Keep the device in clean normal indoor air.");
}

// =====================================================
// Process Calibration Sample
// =====================================================

static bool processCalibrationSample(
    float gasResistance
)
{
    if (!calibrationActive)
    {
        return false;
    }

    unsigned long elapsed =
        millis() - calibrationStartTime;

    // -------------------------------------------------
    // Warm-up period
    // -------------------------------------------------

    if (elapsed < AQ_CALIBRATION_WARMUP_MS)
    {
        return false;
    }

    // -------------------------------------------------
    // Validate gas value
    // -------------------------------------------------

    if (!isfinite(gasResistance) ||
        gasResistance < AQ_CALIBRATION_MIN_GAS)
    {
        return false;
    }

    calibrationGasSum += gasResistance;
    calibrationSampleCount++;

    // -------------------------------------------------
    // Calibration complete
    // -------------------------------------------------

    if (calibrationSampleCount >=
        AQ_CALIBRATION_SAMPLES)
    {
        float calculatedBaseline =
            (float)(
                calibrationGasSum /
                (double)calibrationSampleCount
            );

        if (!saveBaseline(calculatedBaseline))
        {
            Serial.println(
                "Air Quality baseline save FAILED"
            );

            // Keep using the calculated baseline for
            // this boot even if NVS write failed.
        }

        gasBaseline = calculatedBaseline;

        baselineLoaded = true;
        calibrationComplete = true;
        calibrationActive = false;

        Serial.println();
        Serial.println(
            "Air Quality Calibration COMPLETE"
        );

        Serial.print(
            "Gas Baseline : "
        );

        Serial.print(
            gasBaseline,
            2
        );

        Serial.println(
            " kOhm"
        );

        Serial.print(
            "Samples      : "
        );

        Serial.println(
            calibrationSampleCount
        );

        return true;
    }

    return false;
}

// =====================================================
// Initialization
// =====================================================

void initAirQuality()
{
    resetAirQualityFilter();
    Serial.println();
    Serial.println("==============================");
    Serial.println("Air Quality System");
    Serial.println("==============================");

    if (loadBaseline())
    {
        Serial.println(
            "Saved Air Quality baseline found"
        );

        Serial.print(
            "Gas Baseline : "
        );

        Serial.print(
            gasBaseline,
            2
        );

        Serial.println(
            " kOhm"
        );

        Serial.println(
            "Relative air quality calculation ready"
        );
    }
    else
    {
        startCalibration();
    }
}

// =====================================================
// Status Name
// =====================================================

const char* getAirQualityStatusName(
    AirQualityStatus status
)
{
    switch (status)
    {
        case AIR_QUALITY_GOOD:
            return "GOOD";

        case AIR_QUALITY_MODERATE:
            return "MODERATE";

        case AIR_QUALITY_POOR:
            return "POOR";

        case AIR_QUALITY_VERY_POOR:
            return "VERY POOR";

        default:
            return "UNKNOWN";
    }
}

// =====================================================
// Relative Gas Score
// =====================================================
//
// The score is based on gas resistance relative to the
// device-specific baseline:
//
//   >= 100% baseline : clean/reference region
//   90-100%          : slightly below baseline
//   75-90%           : degraded
//   50-75%           : significantly degraded
//   < 50%            : strongly degraded
//
// This is a relative AirSense index, NOT an official AQI.
// =====================================================

static float calculateRelativeGasScore(
    float gasResistance
)
{
    if (!baselineLoaded ||
        gasBaseline <= 0.0f)
    {
        return 0.0f;
    }

    float ratio =
        gasResistance / gasBaseline;

    float score = 0.0f;

    if (ratio >= 1.0f)
    {
        score =
            80.0f +
            ((ratio - 1.0f) * 100.0f);

        score =
            constrain(
                score,
                80.0f,
                100.0f
            );
    }
    else if (ratio >= 0.90f)
    {
        score =
            65.0f +
            ((ratio - 0.90f) / 0.10f)
            * 15.0f;
    }
    else if (ratio >= 0.75f)
    {
        score =
            45.0f +
            ((ratio - 0.75f) / 0.15f)
            * 20.0f;
    }
    else if (ratio >= 0.50f)
    {
        score =
            20.0f +
            ((ratio - 0.50f) / 0.25f)
            * 25.0f;
    }
    else
    {
        score =
            (ratio / 0.50f) * 20.0f;
    }

    return constrain(
        score,
        0.0f,
        100.0f
    );
}

// =====================================================
// Air Quality Calculation
// =====================================================

AirQualityData calculateAirQuality(
    float gasResistance,
    float humidity,
    float temperature
)
{
    AirQualityData result;

    result.status = AIR_QUALITY_UNKNOWN;
    result.score = 0.0f;
    result.rawScore = 0.0f;
    result.gasScore = 0.0f;
    result.baseline = gasBaseline;
    result.level = "UNKNOWN";
    result.message =
        "Waiting for sensor data";

    // -------------------------------------------------
    // Validate sensor values
    // -------------------------------------------------

    if (!isfinite(gasResistance) ||
        !isfinite(humidity) ||
        !isfinite(temperature))
    {
        return result;
    }

    if (gasResistance <= 0.0f)
    {
        return result;
    }

    // -------------------------------------------------
    // Calibration handling
    // -------------------------------------------------

    if (calibrationActive)
    {
        processCalibrationSample(
            gasResistance
        );

        if (!calibrationComplete)
        {
            result.level = "CALIBRATING";
            result.message =
                "Air quality baseline calibration";
            result.score = 0.0f;
            result.rawScore = 0.0f;
            result.gasScore = 0.0f;
            result.baseline = gasBaseline;

            return result;
        }
    }

    if (!baselineLoaded ||
        gasBaseline <= 0.0f)
    {
        result.level = "CALIBRATING";
        result.message =
            "Waiting for gas baseline";

        return result;
    }

    // -------------------------------------------------
    // Filter only on a fresh sensor measurement. API polling
    // can call this function repeatedly without advancing
    // the filter state.
    // -------------------------------------------------

    bool newMeasurement = airQualityInputsChanged(
        gasResistance,
        humidity,
        temperature
    );

    if (newMeasurement)
    {
        lastInputGas = gasResistance;
        lastInputHumidity = humidity;
        lastInputTemperature = temperature;

        float filteredGas = medianGasResistance(gasResistance);

        lastGasScore = calculateRelativeGasScore(filteredGas);

        // -------------------------------------------------
        // Humidity adjustment
        // -------------------------------------------------

        float humidityPenalty = 0.0f;

        if (humidity < HUMIDITY_MIN)
        {
            humidityPenalty = (HUMIDITY_MIN - humidity) * 0.25f;
        }
        else if (humidity > HUMIDITY_MAX)
        {
            humidityPenalty = (humidity - HUMIDITY_MAX) * 0.25f;
        }

        humidityPenalty = constrain(humidityPenalty, 0.0f, 20.0f);

        lastRawScore = constrain(
            lastGasScore - humidityPenalty,
            0.0f,
            100.0f
        );

        if (!isfinite(smoothedScore))
            smoothedScore = lastRawScore;
        else
            smoothedScore += AQ_SCORE_EMA_ALPHA * (lastRawScore - smoothedScore);
    }

    float gasScore = isfinite(lastGasScore) ? lastGasScore : 0.0f;

    // -------------------------------------------------
    // Final relative score
    // -------------------------------------------------

    float finalScore = isfinite(smoothedScore) ? smoothedScore : 0.0f;
    float rawScore = isfinite(lastRawScore) ? lastRawScore : finalScore;

    result.score = finalScore;
    result.rawScore = rawScore;
    result.gasScore = gasScore;
    result.baseline = gasBaseline;

    // -------------------------------------------------
    // Determine air quality level
    // -------------------------------------------------

    if (finalScore >= 75.0f)
    {
        result.status =
            AIR_QUALITY_GOOD;

        result.level =
            "GOOD";

        result.message =
            "Air quality is good";
    }
    else if (finalScore >= 50.0f)
    {
        result.status =
            AIR_QUALITY_MODERATE;

        result.level =
            "MODERATE";

        result.message =
            "Air quality is acceptable";
    }
    else if (finalScore >= 25.0f)
    {
        result.status =
            AIR_QUALITY_POOR;

        result.level =
            "POOR";

        result.message =
            "Air quality needs attention";
    }
    else
    {
        result.status =
            AIR_QUALITY_VERY_POOR;

        result.level =
            "VERY POOR";

        result.message =
            "Poor air quality detected";
    }

    return result;
}
