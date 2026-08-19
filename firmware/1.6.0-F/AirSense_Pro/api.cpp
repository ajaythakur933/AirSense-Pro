#include "api.h"
#include "globals.h"
#include "air_quality.h"
#include "sensor.h"
#include "system_manager.h"
#include "wifi_manager.h"
#include "version.h"
#include "logger.h"
#include "history.h"

#include <WiFi.h>
#include <WebServer.h>
#include <math.h>

extern WebServer server;


void handleHistoryAPI()
{
    // Supported windows: 1h, 6h, 12h, 24h. Default is 6h.
    uint32_t windowSeconds = 6UL * 60UL * 60UL;

    if (server.hasArg("window"))
    {
        String window = server.arg("window");
        if (window == "1h") windowSeconds = 1UL * 60UL * 60UL;
        else if (window == "12h") windowSeconds = 12UL * 60UL * 60UL;
        else if (window == "24h") windowSeconds = 24UL * 60UL * 60UL;
    }

    uint16_t total = historyGetCount();
    uint16_t count = historyGetWindowCount(windowSeconds);
    uint16_t start = total > count ? total - count : 0;

    float tMin = INFINITY, tMax = -INFINITY, tSum = 0;
    float hMin = INFINITY, hMax = -INFINITY, hSum = 0;
    float pMin = INFINITY, pMax = -INFINITY, pSum = 0;
    float gMin = INFINITY, gMax = -INFINITY, gSum = 0;
    float aMin = INFINITY, aMax = -INFINITY, aSum = 0;
    uint16_t valid = 0;

    String json;
    json.reserve(6000 + (size_t)count * 150);
    json += "{\"firmware\":\"" + String(PRODUCT_VERSION) + "\",";
    json += "\"sampleIntervalSeconds\":" + String(historyGetSampleIntervalSeconds()) + ",";
    json += "\"count\":" + String(count) + ",";
    json += "\"totalCount\":" + String(total) + ",";
    json += "\"capacity\":" + String(historyGetCapacity()) + ",";
    json += "\"samples\":[";

    bool firstJson = true;
    for (uint16_t i = start; i < total; ++i)
    {
        HistorySample sample;
        if (!historyGetSample(i, sample)) continue;

        if (!firstJson) json += ",";
        firstJson = false;
        json += "{\"sequence\":" + String(historyGetSampleSequence(i));
        json += ",\"uptime\":" + String(sample.uptimeSeconds);
        json += ",\"temperature\":" + String(sample.temperature, 2);
        json += ",\"humidity\":" + String(sample.humidity, 2);
        json += ",\"pressure\":" + String(sample.pressure, 2);
        json += ",\"gasResistance\":" + String(sample.gasResistance, 2);
        json += ",\"airQualityScore\":" + String(sample.airQualityScore, 1) + "}";

        if (isfinite(sample.temperature)) { tMin=min(tMin,sample.temperature); tMax=max(tMax,sample.temperature); tSum+=sample.temperature; }
        if (isfinite(sample.humidity)) { hMin=min(hMin,sample.humidity); hMax=max(hMax,sample.humidity); hSum+=sample.humidity; }
        if (isfinite(sample.pressure)) { pMin=min(pMin,sample.pressure); pMax=max(pMax,sample.pressure); pSum+=sample.pressure; }
        if (isfinite(sample.gasResistance)) { gMin=min(gMin,sample.gasResistance); gMax=max(gMax,sample.gasResistance); gSum+=sample.gasResistance; }
        if (isfinite(sample.airQualityScore)) { aMin=min(aMin,sample.airQualityScore); aMax=max(aMax,sample.airQualityScore); aSum+=sample.airQualityScore; }
        valid++;
    }

    json += "],\"statistics\":{";
    if (valid > 0)
    {
        json += "\"temperature\":{\"min\":" + String(tMin,2) + ",\"avg\":" + String(tSum/valid,2) + ",\"max\":" + String(tMax,2) + "},";
        json += "\"humidity\":{\"min\":" + String(hMin,2) + ",\"avg\":" + String(hSum/valid,2) + ",\"max\":" + String(hMax,2) + "},";
        json += "\"pressure\":{\"min\":" + String(pMin,2) + ",\"avg\":" + String(pSum/valid,2) + ",\"max\":" + String(pMax,2) + "},";
        json += "\"gasResistance\":{\"min\":" + String(gMin,2) + ",\"avg\":" + String(gSum/valid,2) + ",\"max\":" + String(gMax,2) + "},";
        json += "\"airQualityScore\":{\"min\":" + String(aMin,1) + ",\"avg\":" + String(aSum/valid,1) + ",\"max\":" + String(aMax,1) + "}";
    }
    json += "}}";

    server.send(200, "application/json; charset=UTF-8", json);
}

void handleSensorAPI()
{
    String json = "{";

    json += "\"temperature\":";
    json += String(temperature, 2);
    json += ",";
    json += "\"humidity\":";
    json += String(humidity, 2);
    json += ",";
    json += "\"pressure\":";
    json += String(pressure, 2);
    json += ",";
    json += "\"gasResistance\":";
    json += String(gasResistance, 2);

    AirQualityData airQuality =
        calculateAirQuality(gasResistance, humidity, temperature);

    json += ",\"airQualityScore\":";
    json += String(airQuality.score, 1);
    json += ",\"airQualityRawScore\":";
    json += String(airQuality.rawScore, 1);
    json += ",\"airQualityGasScore\":";
    json += String(airQuality.gasScore, 1);
    json += ",\"airQualityBaseline\":";
    json += String(airQuality.baseline, 2);
    json += ",\"airQualityStatus\":\"";
    json += airQuality.level;
    json += "\"";
    json += ",\"airQualityMessage\":\"";
    json += airQuality.message;
    json += "\"";

    json += "}";

    server.send(200, "application/json; charset=UTF-8", json);
}

void handleSystemAPI()
{
    String json = "{";

    json += "\"wifi\":";
    json += (WiFi.status() == WL_CONNECTED ? "true" : "false");

    json += ",\"ssid\":\"";
    json += WiFi.SSID();
    json += "\"";

    json += ",\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\"";

    json += ",\"rssi\":";
    json += String(WiFi.RSSI());

    // V1.5.0-B Wi-Fi reliability diagnostics
    json += ",\"wifiDisconnects\":";
    json += String(wifiGetDisconnectCount());

    json += ",\"wifiReconnectAttempts\":";
    json += String(wifiGetReconnectAttemptCount());

    json += ",\"wifiReconnectSuccess\":";
    json += String(wifiGetReconnectSuccessCount());

    json += ",\"wifiReconnectFailures\":";
    json += String(wifiGetReconnectFailureCount());

    json += ",\"tcpLogClient\":";
    json += (loggerClientConnected() ? "true" : "false");

    json += ",\"tcpLogConnections\":";
    json += String(loggerGetConnectionCount());

    json += ",\"tcpLogDisconnects\":";
    json += String(loggerGetDisconnectCount());

    json += ",\"firmware\":\"";
    json += PRODUCT_VERSION;
    json += "\"";

    json += ",\"uptime\":";
    json += String(systemUptimeSeconds());

    json += ",\"freeHeap\":";
    json += String(systemFreeHeap());

    json += ",\"sensorReads\":";
    json += String(sensorGetReadCount());

    json += ",\"sensorFailures\":";
    json += String(sensorGetFailureCount());

    json += ",\"sensorRecoveries\":";
    json += String(sensorGetRecoveryCount());

    json += ",\"sensorStaleEvents\":";
    json += String(sensorGetStaleCount());

    json += ",\"sensorRecoveryAttempts\":";
    json += String(sensorGetRecoveryAttemptCount());

    json += ",\"sensorOfflineEvents\":";
    json += String(sensorGetOfflineEventCount());

    json += ",\"sensorRecoveryAttemptsSinceFailure\":";
    json += String(sensorGetRecoveryAttemptsSinceFailure());

    json += ",\"sensorOffline\":";
    json += (sensorIsOffline() ? "true" : "false");

    json += ",\"sensorHealthy\":";
    json += (sensorIsHealthy() ? "true" : "false");

    json += ",\"historyReady\":";
    json += (historyIsReady() ? "true" : "false");

    json += ",\"historySamples\":";
    json += String(historyGetCount());

    json += ",\"historyCapacity\":";
    json += String(historyGetCapacity());

    json += ",\"historySampleIntervalSeconds\":";
    json += String(historyGetSampleIntervalSeconds());

    json += ",\"historyLastSampleUptime\":";
    json += String(historyGetLastSampleUptime());

    json += "}";

    server.send(200, "application/json; charset=UTF-8", json);
}
