#include "history.h"
#include "logger.h"

#include <Preferences.h>

namespace
{
    // One sample every 5 minutes. 288 samples = 24 hours.
    static const uint32_t SAMPLE_INTERVAL_SECONDS = 300UL;
    static const uint16_t HISTORY_CAPACITY = 288;

    // Persist every two samples (~5 minutes) to reduce flash wear.
    static const uint16_t PERSIST_EVERY_SAMPLES = 2;

    // Four NVS chunks keep every Preferences value below the ESP32 limit.
    static const uint16_t CHUNK_SAMPLES = 72;
    static const uint16_t HISTORY_CHUNKS = 4;

    // Keep each NVS blob comfortably below the ESP32 Preferences value limit.
    static const uint16_t HISTORY_STORAGE_VERSION = 2;

    Preferences historyPreferences;

    HistorySample samples[HISTORY_CAPACITY];
    uint16_t sampleCount = 0;
    uint16_t writeIndex = 0;
    uint32_t nextSequence = 1;
    uint32_t sampleSequences[HISTORY_CAPACITY];

    uint32_t lastSampleUptime = 0;
    uint16_t samplesSincePersist = 0;
    bool ready = false;
    bool dirty = false;

    static void clearHistory()
    {
        sampleCount = 0;
        writeIndex = 0;
        lastSampleUptime = 0;
        nextSequence = 1;
        samplesSincePersist = 0;
        dirty = false;
        memset(samples, 0, sizeof(samples));
        memset(sampleSequences, 0, sizeof(sampleSequences));
    }

    // Keep the history buffer circular when full.
    // The previous v1.6.0-E implementation copied the entire 288-sample
    // buffer into two large local arrays here. That consumed several KB of
    // loopTask stack and caused:
    //   Stack canary watchpoint triggered (loopTask)
    // on the first sample after a reboot when history was already full.
    // v1.6.0-F exposes chronological order through historyGetSample()
    // instead, so no large temporary arrays are needed.

    static bool persistHistory()
    {
        bool ok = true;

        if (historyPreferences.putUShort("ver", HISTORY_STORAGE_VERSION) != HISTORY_STORAGE_VERSION)
            ok = false;
        if (historyPreferences.putUShort("count", sampleCount) != sampleCount)
            ok = false;
        if (historyPreferences.putUShort("index", writeIndex) != writeIndex)
            ok = false;
        if (historyPreferences.putUInt("next", nextSequence) != nextSequence)
            ok = false;
        if (historyPreferences.putUInt("last", lastSampleUptime) != lastSampleUptime)
            ok = false;

        for (uint16_t chunk = 0; chunk < HISTORY_CHUNKS; ++chunk)
        {
            uint16_t start = chunk * CHUNK_SAMPLES;
            uint16_t remaining = (sampleCount > start) ? (sampleCount - start) : 0;
            uint16_t count = remaining > CHUNK_SAMPLES ? CHUNK_SAMPLES : remaining;
            char key[4];
            snprintf(key, sizeof(key), "h%u", chunk);

            size_t written = historyPreferences.putBytes(
                key,
                count ? (const void *)(samples + start) : nullptr,
                count * sizeof(HistorySample));

            if (written != count * sizeof(HistorySample))
                ok = false;

            char seqKey[5];
            snprintf(seqKey, sizeof(seqKey), "s%u", chunk);
            written = historyPreferences.putBytes(
                seqKey,
                count ? (const void *)(sampleSequences + start) : nullptr,
                count * sizeof(uint32_t));

            if (written != count * sizeof(uint32_t))
                ok = false;
        }

        if (ok)
        {
            samplesSincePersist = 0;
            dirty = false;
        }

        return ok;
    }

    static bool migrateLegacyHistory(uint16_t storedCount)
    {
        if (storedCount == 0 || storedCount > HISTORY_CAPACITY)
            return false;

        size_t firstCount = storedCount < 144 ? storedCount : 144;
        size_t secondCount = storedCount > 144 ? storedCount - 144 : 0;
        bool firstOk = historyPreferences.getBytesLength("h0") == firstCount * sizeof(HistorySample);
        bool secondOk = (secondCount == 0) ||
                        (historyPreferences.getBytesLength("h1") == secondCount * sizeof(HistorySample));

        if (!firstOk || !secondOk)
            return false;

        historyPreferences.getBytes("h0", samples, firstCount * sizeof(HistorySample));
        if (secondCount)
            historyPreferences.getBytes("h1", samples + 144, secondCount * sizeof(HistorySample));

        sampleCount = storedCount;
        writeIndex = sampleCount < HISTORY_CAPACITY ? sampleCount : 0;
        for (uint16_t i = 0; i < sampleCount; ++i)
            sampleSequences[i] = i + 1;
        nextSequence = sampleCount + 1;
        lastSampleUptime = historyPreferences.getUInt("last", 0);
        return true;
    }

    static void loadHistory()
    {
        clearHistory();

        uint16_t version = historyPreferences.getUShort("ver", 0);
        uint16_t storedCount = historyPreferences.getUShort("count", 0);
        uint16_t storedIndex = historyPreferences.getUShort("index", 0);

        // V1.6.0-B used oversized NVS blobs. Start a clean storage layout
        // when upgrading so a corrupt count cannot expose zero-filled slots.
        if (version != HISTORY_STORAGE_VERSION)
        {
            // Try to preserve a valid V1.6.0-B history first. If the old
            // oversized NVS blobs are incomplete, start clean rather than
            // exposing zero-filled entries.
            if (migrateLegacyHistory(storedCount))
            {
                persistHistory();
                return;
            }

            historyPreferences.clear();
            return;
        }

        if (storedCount > HISTORY_CAPACITY)
            storedCount = 0;
        if (storedIndex >= HISTORY_CAPACITY)
            storedIndex = 0;

        for (uint16_t chunk = 0; chunk < HISTORY_CHUNKS; ++chunk)
        {
            uint16_t start = chunk * CHUNK_SAMPLES;
            uint16_t remaining = (storedCount > start) ? (storedCount - start) : 0;
            uint16_t count = remaining > CHUNK_SAMPLES ? CHUNK_SAMPLES : remaining;

            char key[4];
            snprintf(key, sizeof(key), "h%u", chunk);
            if (count)
                historyPreferences.getBytes(key, samples + start, count * sizeof(HistorySample));

            char seqKey[5];
            snprintf(seqKey, sizeof(seqKey), "s%u", chunk);
            if (count)
                historyPreferences.getBytes(seqKey, sampleSequences + start, count * sizeof(uint32_t));
        }

        sampleCount = storedCount;
        writeIndex = storedIndex;
        lastSampleUptime = historyPreferences.getUInt("last", 0);
        nextSequence = historyPreferences.getUInt("next", 1);

        if (sampleCount > 0 && nextSequence <= sampleSequences[sampleCount - 1])
            nextSequence = sampleSequences[sampleCount - 1] + 1;

        // Legacy/partial sequence data: create a deterministic sequence.
        bool missingSequence = false;
        for (uint16_t i = 0; i < sampleCount; ++i)
        {
            if (sampleSequences[i] == 0)
            {
                missingSequence = true;
                sampleSequences[i] = i + 1;
            }
        }
        if (missingSequence)
            nextSequence = sampleCount + 1;

        // For a partially filled buffer the next write is append-at-count.
        if (sampleCount < HISTORY_CAPACITY)
            writeIndex = sampleCount;
    }
}

bool initHistory()
{
    if (!historyPreferences.begin("history16", false))
    {
        loggerPrintln("History storage initialization FAILED");
        ready = false;
        return false;
    }

    loadHistory();
    ready = true;

    loggerPrint("History storage ready | Samples: ");
    loggerPrint(sampleCount);
    loggerPrint(" / ");
    loggerPrintln(HISTORY_CAPACITY);

    return true;
}

void historyRecord(float newTemperature,
                   float newHumidity,
                   float newPressure,
                   float newGasResistance,
                   float newAirQualityScore)
{
    if (!ready)
    {
        return;
    }

    uint32_t nowSeconds = millis() / 1000UL;

    // Record only at the configured interval.
    if (lastSampleUptime != 0 &&
        nowSeconds - lastSampleUptime < SAMPLE_INTERVAL_SECONDS)
    {
        return;
    }

    HistorySample sample;
    sample.uptimeSeconds = nowSeconds;
    sample.temperature = newTemperature;
    sample.humidity = newHumidity;
    sample.pressure = newPressure;
    sample.gasResistance = newGasResistance;
    sample.airQualityScore = newAirQualityScore;

    if (sampleCount < HISTORY_CAPACITY)
    {
        samples[sampleCount] = sample;
        sampleSequences[sampleCount] = nextSequence++;
        sampleCount++;
        writeIndex = sampleCount % HISTORY_CAPACITY;
    }
    else
    {
        samples[writeIndex] = sample;
        sampleSequences[writeIndex] = nextSequence++;
        writeIndex = (writeIndex + 1) % HISTORY_CAPACITY;

        // Keep the buffer circular. Chronological ordering is handled by
        // historyGetSample() / historyGetSampleSequence(), avoiding large
        // stack allocations when the 24-hour buffer is full.
    }

    lastSampleUptime = nowSeconds;
    samplesSincePersist++;
    dirty = true;

    if (samplesSincePersist >= PERSIST_EVERY_SAMPLES)
    {
        if (!persistHistory())
        {
            loggerPrintln("History persistence FAILED");
        }
    }
}

void historyLoop()
{
    // Nothing is intentionally written here. Persistence is triggered
    // by sampleRecord() every two samples.
}

uint16_t historyGetCount()
{
    return sampleCount;
}

uint16_t historyGetCapacity()
{
    return HISTORY_CAPACITY;
}

uint16_t historyGetWindowCount(uint32_t windowSeconds)
{
    if (sampleCount == 0)
        return 0;

    // Uptime resets after reboot, so window filtering must not depend on it.
    // Use the configured sample interval instead. Include both endpoints.
    uint32_t intervals = windowSeconds / SAMPLE_INTERVAL_SECONDS;
    uint32_t wanted = intervals + 1;
    if (wanted > HISTORY_CAPACITY)
        wanted = HISTORY_CAPACITY;
    if (wanted > sampleCount)
        wanted = sampleCount;

    return (uint16_t)wanted;
}

const HistorySample* historyGetSamples()
{
    return samples;
}

bool historyGetSample(uint16_t index, HistorySample &sample)
{
    if (index >= sampleCount)
    {
        return false;
    }

    // When the buffer is full, writeIndex points to the oldest physical
    // slot. Present samples to callers in chronological order without
    // physically rebuilding the array.
    uint16_t physicalIndex = index;
    if (sampleCount >= HISTORY_CAPACITY)
    {
        physicalIndex = (uint16_t)((writeIndex + index) % HISTORY_CAPACITY);
    }

    sample = samples[physicalIndex];
    return true;
}

uint32_t historyGetSampleSequence(uint16_t index)
{
    if (index >= sampleCount)
        return 0;

    uint16_t physicalIndex = index;
    if (sampleCount >= HISTORY_CAPACITY)
    {
        physicalIndex = (uint16_t)((writeIndex + index) % HISTORY_CAPACITY);
    }

    return sampleSequences[physicalIndex];
}

uint32_t historyGetSampleIntervalSeconds()
{
    return SAMPLE_INTERVAL_SECONDS;
}

uint32_t historyGetLastSampleUptime()
{
    return lastSampleUptime;
}

bool historyIsReady()
{
    return ready;
}
