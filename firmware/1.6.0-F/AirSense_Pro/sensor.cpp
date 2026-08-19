#include <Wire.h>
#include <Adafruit_BME680.h>

#include "config.h"
#include "globals.h"
#include "logger.h"

Adafruit_BME680 bme;

// =====================================================
// Sensor Reliability Configuration
// =====================================================

// Read BME680 approximately once per second.
// The web dashboard may poll independently.
static const unsigned long SENSOR_READ_INTERVAL_MS = 1000;

// Number of consecutive failed readings before marking
// the BME680 for recovery.
static const uint8_t SENSOR_MAX_CONSECUTIVE_FAILURES = 3;

// Minimum delay between sensor recovery attempts while retrying.
static const unsigned long SENSOR_RECOVERY_INTERVAL_MS = 5000;

// Maximum number of recovery attempts in one failure episode.
// Each recovery attempt may internally try BME680 initialization twice.
static const uint8_t SENSOR_MAX_RECOVERY_ATTEMPTS = 3;

// Once the maximum recovery attempts are exhausted, keep the sensor
// offline and retry only periodically. This prevents a tight recovery loop.
static const unsigned long SENSOR_OFFLINE_RETRY_INTERVAL_MS = 30000;

// NEW - Phase 2:
// Number of consecutive VALID readings with exactly the
// same environmental values before considering the sensor
// output stale.
//
// We intentionally use exact float equality here instead of
// loose tolerances. This avoids false recovery when the room
// is genuinely stable but values move slightly.
static const uint8_t SENSOR_MAX_STALE_READINGS = 30;

// =====================================================
// Sensor State
// =====================================================

static bool sensorInitialized = false;

static unsigned long lastSensorRead = 0;
static unsigned long lastSensorRecovery = 0;

static uint32_t sensorReadCount = 0;
static uint32_t sensorFailureCount = 0;
static uint8_t consecutiveFailures = 0;

// NEW - Phase 2 stale-data state
static uint8_t consecutiveStaleReadings = 0;
static bool havePreviousReading = false;

static float previousTemperature = 0.0f;
static float previousHumidity = 0.0f;
static float previousPressure = 0.0f;
static float previousGasResistance = 0.0f;

// Useful diagnostic counters
static uint32_t sensorRecoveryCount = 0;
static uint32_t sensorStaleCount = 0;

// V1.5.0-B2-C recovery state.
// sensorRecoveryCount = successful recoveries.
static uint8_t recoveryAttemptsSinceFailure = 0;
static uint32_t sensorRecoveryAttemptCount = 0;
static uint32_t sensorOfflineEventCount = 0;
static bool sensorOffline = false;

// =====================================================
// Configure BME680
// =====================================================

static bool configureSensor()
{
    // Keep I2C transactions bounded so a stuck bus does not
    // block the main loop indefinitely.
    Wire.setTimeOut(50);

    if (!bme.begin(0x77))
    {
        return false;
    }

    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

    // 320°C heater for 150 ms.
    bme.setGasHeater(320, 150);

    return true;
}

// =====================================================
// Initialize Sensor
// =====================================================

bool initSensor()
{
    loggerPrintln("Initializing BME680...");

    if (!configureSensor())
    {
        sensorInitialized = false;

        loggerPrintln("BME680 Initialization FAILED");

        return false;
    }

    sensorInitialized = true;

    lastSensorRead = 0;
    lastSensorRecovery = millis();

    sensorReadCount = 0;
    sensorFailureCount = 0;
    consecutiveFailures = 0;

    // NEW - reset stale detection state
    consecutiveStaleReadings = 0;
    havePreviousReading = false;

    sensorRecoveryCount = 0;
    sensorStaleCount = 0;
    recoveryAttemptsSinceFailure = 0;
    sensorRecoveryAttemptCount = 0;
    sensorOfflineEventCount = 0;
    sensorOffline = false;

    loggerPrintln("BME680 Initialization SUCCESS");

    return true;
}

// =====================================================
// Sensor Value Validation
// =====================================================

static bool validateSensorValues(
    float newTemperature,
    float newHumidity,
    float newPressure,
    float newGasResistance
)
{
    if (isnan(newTemperature) ||
        isnan(newHumidity) ||
        isnan(newPressure) ||
        isnan(newGasResistance))
    {
        return false;
    }

    // Basic sanity checks.
    if (newTemperature < -40.0f ||
        newTemperature > 85.0f)
    {
        return false;
    }

    if (newHumidity < 0.0f ||
        newHumidity > 100.0f)
    {
        return false;
    }

    if (newPressure <= 0.0f)
    {
        return false;
    }

    if (newGasResistance <= 0.0f)
    {
        return false;
    }

    return true;
}

// =====================================================
// I2C Bus Recovery
// =====================================================

static void recoverI2CBus()
{
    loggerPrintln("I2C bus recovery started");

    // Release the I2C peripheral before manually clocking SCL.
    Wire.end();

    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);

    // Give a slave that is holding SDA low up to 9 clock pulses
    // to finish an interrupted transaction.
    for (uint8_t i = 0;
         i < 9 && digitalRead(SDA_PIN) == LOW;
         i++)
    {
        pinMode(SCL_PIN, OUTPUT);
        digitalWrite(SCL_PIN, LOW);
        delayMicroseconds(5);

        pinMode(SCL_PIN, INPUT_PULLUP);
        delayMicroseconds(5);
    }

    // Generate an I2C STOP condition.
    pinMode(SDA_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);
    delayMicroseconds(5);

    pinMode(SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(5);

    pinMode(SDA_PIN, INPUT_PULLUP);
    delayMicroseconds(5);

    // Recreate the ESP32 I2C peripheral.
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setTimeOut(50);

    loggerPrintln("I2C bus recovery complete");
}

// =====================================================
// Reset Stale Detection State
// =====================================================

static void resetStaleDetection()
{
    consecutiveStaleReadings = 0;
    havePreviousReading = false;
}

// =====================================================
// Sensor Recovery
// =====================================================

static bool recoverSensor()
{
    unsigned long now = millis();

    // Use a longer retry interval while the sensor is considered offline.
    unsigned long retryInterval =
        sensorOffline ?
        SENSOR_OFFLINE_RETRY_INTERVAL_MS :
        SENSOR_RECOVERY_INTERVAL_MS;

    if (now - lastSensorRecovery < retryInterval)
    {
        return false;
    }

    lastSensorRecovery = now;

    // Once offline, periodic retries do not create a new recovery episode
    // and do not increase the per-episode attempt number beyond the limit.
    bool offlineRetry = sensorOffline;

    if (!offlineRetry)
    {
        recoveryAttemptsSinceFailure++;
    }

    sensorRecoveryAttemptCount++;

    loggerPrintln("");
    loggerPrintln("==============================");
    if (offlineRetry)
    {
        loggerPrintln("BME680 Offline Retry");
    }
    else
    {
        loggerPrint("BME680 Recovery Attempt #");
        loggerPrintln(recoveryAttemptsSinceFailure);
    }
    loggerPrintln("==============================");

    sensorInitialized = false;

    // First try a clean I2C bus recovery. This is important when
    // the BME680 or bus is left in the middle of an I2C transaction.
    recoverI2CBus();

    if (!configureSensor())
    {
        loggerPrintln("BME680 Recovery: first initialization FAILED");
        loggerPrintln("BME680 Recovery: retrying after second I2C reset");

        recoverI2CBus();

        if (!configureSensor())
        {
            loggerPrintln("BME680 Recovery FAILED");

            if (!offlineRetry &&
                recoveryAttemptsSinceFailure >= SENSOR_MAX_RECOVERY_ATTEMPTS)
            {
                sensorOffline = true;
                sensorOfflineEventCount++;

                loggerPrintln("BME680 maximum recovery attempts reached");
                loggerPrintln("BME680 SENSOR OFFLINE");
                loggerPrint("Next recovery retry in ");
                loggerPrint(SENSOR_OFFLINE_RETRY_INTERVAL_MS / 1000UL);
                loggerPrintln(" seconds");
            }
            else if (offlineRetry)
            {
                // Remain offline and keep the retry cadence throttled.
                loggerPrint("BME680 remains OFFLINE | Next retry in ");
                loggerPrint(SENSOR_OFFLINE_RETRY_INTERVAL_MS / 1000UL);
                loggerPrintln(" seconds");
            }
            else
            {
                loggerPrint("Next recovery attempt in ");
                loggerPrint(SENSOR_RECOVERY_INTERVAL_MS / 1000UL);
                loggerPrintln(" seconds");
            }

            return false;
        }
    }

    sensorInitialized = true;
    sensorOffline = false;

    consecutiveFailures = 0;
    recoveryAttemptsSinceFailure = 0;
    lastSensorRead = millis();

    // After recovery, the first fresh reading becomes the new reference point.
    resetStaleDetection();

    sensorRecoveryCount++;

    loggerPrint("BME680 Recovery SUCCESS | Recovery count: ");
    loggerPrintln(sensorRecoveryCount);

    return true;
}

// =====================================================
// Update Sensor
// =====================================================

bool updateSensor()
{
    unsigned long now = millis();

    // Prevent continuous performReading() calls.
    if (lastSensorRead != 0 &&
        now - lastSensorRead < SENSOR_READ_INTERVAL_MS)
    {
        return false;
    }

    lastSensorRead = now;

    // If the sensor is not currently initialized,
    // periodically attempt recovery.
    if (!sensorInitialized)
    {
        if (!recoverSensor())
        {
            return false;
        }
    }

    // -------------------------------------------------
    // Perform BME680 measurement
    // -------------------------------------------------

    if (!bme.performReading())
    {
        sensorFailureCount++;
        consecutiveFailures++;

        loggerPrint("BME680 reading FAILED | Consecutive failures: ");
        loggerPrint(consecutiveFailures);
        loggerPrint(" | Total failures: ");
        loggerPrintln(sensorFailureCount);

        if (consecutiveFailures >= SENSOR_MAX_CONSECUTIVE_FAILURES)
        {
            sensorInitialized = false;

            // Start a new recovery episode only once.
            if (recoveryAttemptsSinceFailure == 0)
            {
                sensorOffline = false;
            }

            loggerPrintln(
                "BME680 consecutive failure threshold reached"
            );

            loggerPrintln(
                "Sensor marked for recovery"
            );
        }

        // Keep the previous valid environmental values.
        return false;
    }

    // -------------------------------------------------
    // Read values into temporary variables first
    // -------------------------------------------------

    float newTemperature = bme.temperature;
    float newHumidity = bme.humidity;
    float newPressure = bme.pressure / 100.0f;
    float newGasResistance = bme.gas_resistance / 1000.0f;

    // -------------------------------------------------
    // Validate measurement
    // -------------------------------------------------

    if (!validateSensorValues(
            newTemperature,
            newHumidity,
            newPressure,
            newGasResistance))
    {
        sensorFailureCount++;
        consecutiveFailures++;

        loggerPrint(
            "BME680 invalid measurement | Consecutive failures: "
        );
        loggerPrintln(consecutiveFailures);

        if (consecutiveFailures >= SENSOR_MAX_CONSECUTIVE_FAILURES)
        {
            sensorInitialized = false;

            // Start a new recovery episode only once.
            if (recoveryAttemptsSinceFailure == 0)
            {
                sensorOffline = false;
            }

            loggerPrintln(
                "BME680 invalid measurement threshold reached"
            );

            loggerPrintln(
                "Sensor marked for recovery"
            );
        }

        return false;
    }

    // -------------------------------------------------
    // NEW - Phase 2: Stale Data Detection
    // -------------------------------------------------

    if (havePreviousReading)
    {
        bool unchanged =
            (newTemperature == previousTemperature) &&
            (newHumidity == previousHumidity) &&
            (newPressure == previousPressure) &&
            (newGasResistance == previousGasResistance);

        if (unchanged)
        {
            consecutiveStaleReadings++;

            loggerPrint("BME680 reading unchanged | Stale count: ");
            loggerPrint(consecutiveStaleReadings);
            loggerPrint("/");
            loggerPrintln(SENSOR_MAX_STALE_READINGS);

            if (consecutiveStaleReadings >= SENSOR_MAX_STALE_READINGS)
            {
                sensorStaleCount++;

                loggerPrintln("");
                loggerPrintln("==============================");
                loggerPrintln("BME680 STALE DATA DETECTED");
                loggerPrintln("==============================");

                loggerPrint("Stale readings: ");
                loggerPrintln(consecutiveStaleReadings);

                loggerPrintln("Sensor marked for recovery");

                // Do not commit another stale reading.
                // Force the existing recovery mechanism to run.
                sensorInitialized = false;

                // A stale-data event starts a fresh recovery episode.
                recoveryAttemptsSinceFailure = 0;
                sensorOffline = false;

                resetStaleDetection();

                return false;
            }
        }
        else
        {
            // Sensor values changed normally.
            consecutiveStaleReadings = 0;
        }
    }

    // Save this valid reading as the next comparison reference.
    previousTemperature = newTemperature;
    previousHumidity = newHumidity;
    previousPressure = newPressure;
    previousGasResistance = newGasResistance;

    havePreviousReading = true;

    // -------------------------------------------------
    // Commit valid measurement
    // -------------------------------------------------

    temperature = newTemperature;
    humidity = newHumidity;
    pressure = newPressure;
    gasResistance = newGasResistance;

    sensorReadCount++;
    consecutiveFailures = 0;

    loggerPrint("Sensor OK | T: ");
    loggerPrint(temperature, 2);

    loggerPrint(" C | H: ");
    loggerPrint(humidity, 2);

    loggerPrint(" % | P: ");
    loggerPrint(pressure, 2);

    loggerPrint(" hPa | Gas: ");
    loggerPrint(gasResistance, 2);

    loggerPrint(" kOhm | Reads: ");
    loggerPrintln(sensorReadCount);

    return true;
}


// =====================================================
// Device Health Diagnostics - V1.5.0-A
// Read-only access to existing sensor state.
// =====================================================

uint32_t sensorGetReadCount()
{
    return sensorReadCount;
}

uint32_t sensorGetFailureCount()
{
    return sensorFailureCount;
}

uint32_t sensorGetRecoveryCount()
{
    return sensorRecoveryCount;
}

uint32_t sensorGetStaleCount()
{
    return sensorStaleCount;
}

uint32_t sensorGetRecoveryAttemptCount()
{
    return sensorRecoveryAttemptCount;
}

uint32_t sensorGetOfflineEventCount()
{
    return sensorOfflineEventCount;
}

uint8_t sensorGetRecoveryAttemptsSinceFailure()
{
    return recoveryAttemptsSinceFailure;
}

bool sensorIsOffline()
{
    return sensorOffline;
}

bool sensorIsHealthy()
{
    return sensorInitialized && (consecutiveFailures == 0);
}
