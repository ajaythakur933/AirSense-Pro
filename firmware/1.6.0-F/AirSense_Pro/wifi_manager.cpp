#include "wifi_manager.h"

#include <WiFi.h>

#include "storage.h"
#include "globals.h"


// =====================================================
// AirSense Pro - Wi-Fi Reliability Manager
// Version 1.5.0-B
// =====================================================

// Retry spacing between asynchronous reconnect attempts.
static const unsigned long WIFI_RETRY_INTERVAL = 10000UL;

// Maximum time allowed for one reconnect attempt before
// counting it as a failed recovery attempt.
static const unsigned long WIFI_RECONNECT_TIMEOUT = 15000UL;

// Pause after a complete retry cycle.
static const unsigned long WIFI_RETRY_PAUSE = 30000UL;

static const int MAX_RECONNECT_ATTEMPTS = 5;


// =====================================================
// State
// =====================================================

static unsigned long lastReconnectAttempt = 0;
static unsigned long reconnectStartTime = 0;

static int reconnectAttempts = 0;

static bool retryPause = false;
static unsigned long retryPauseStart = 0;

static bool wasConnected = false;
static bool reconnectPending = false;

// One-shot event consumed by the main application after a
// previously connected link comes back.
static bool reconnectEventPending = false;


// =====================================================
// Reliability Counters
// =====================================================

static uint32_t wifiDisconnectCount = 0;
static uint32_t wifiReconnectAttemptCount = 0;
static uint32_t wifiReconnectSuccessCount = 0;
static uint32_t wifiReconnectFailureCount = 0;


// =====================================================
// Initialize Wi-Fi
// =====================================================

bool initWiFi()
{
    WiFi.mode(WIFI_STA);

    WiFi.setAutoReconnect(true);
    WiFi.setHostname("AirSense-Pro");

    wifiConnected = false;
    ipAddress = "";
    wifiRSSI = 0;

    return true;
}


// =====================================================
// Connect Using Saved Credentials
// =====================================================

bool connectToSavedWiFi()
{
    if (!isWiFiConfigured())
    {
        Serial.println("No saved WiFi credentials");
        return false;
    }

    String ssid = getWiFiSSID();
    String password = getWiFiPassword();

    Serial.println();
    Serial.println("==============================");
    Serial.println("Connecting to Saved Wi-Fi");
    Serial.println("==============================");

    Serial.print("SSID : ");
    Serial.println(ssid);

    Serial.println("Password : ********");
    Serial.println("Connecting...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    WiFi.begin(
        ssid.c_str(),
        password.c_str()
    );

    unsigned long startTime = millis();
    const unsigned long timeout = 15000UL;

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < timeout
    )
    {
        delay(250);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        reconnectAttempts = 0;
        retryPause = false;
        reconnectPending = false;
        lastReconnectAttempt = millis();
        wasConnected = true;

        wifiConnected = true;
        ipAddress = WiFi.localIP().toString();
        wifiRSSI = WiFi.RSSI();

        Serial.println("Wi-Fi Connected!");

        Serial.print("IP Address : ");
        Serial.println(WiFi.localIP());

        Serial.print("RSSI : ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");

        return true;
    }

    wifiConnected = false;
    ipAddress = "";
    wifiRSSI = 0;

    Serial.println("Wi-Fi Connection Failed");

    WiFi.disconnect();

    return false;
}


// =====================================================
// Wi-Fi Status
// =====================================================

bool isWiFiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}


// =====================================================
// Wi-Fi Monitoring / Auto Reconnect
// =====================================================

void updateWiFi()
{
    wl_status_t currentStatus = WiFi.status();


    // =================================================
    // Connected
    // =================================================

    if (currentStatus == WL_CONNECTED)
    {
        wifiConnected = true;
        ipAddress = WiFi.localIP().toString();
        wifiRSSI = WiFi.RSSI();

        // Connection has just been established.
        if (!wasConnected)
        {
            wasConnected = true;

            reconnectAttempts = 0;
            retryPause = false;

            if (reconnectPending)
            {
                reconnectPending = false;
                wifiReconnectSuccessCount++;
                reconnectEventPending = true;

                Serial.print(
                    "Wi-Fi Recovery SUCCESS | Total successes: "
                );
                Serial.println(
                    wifiReconnectSuccessCount
                );
            }

            Serial.println();
            Serial.println("==============================");
            Serial.println("Wi-Fi Connected!");
            Serial.println("==============================");

            Serial.print("IP Address : ");
            Serial.println(WiFi.localIP());

            Serial.print("RSSI : ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        }

        return;
    }


    // =================================================
    // Connection Lost
    // =================================================

    if (wasConnected)
    {
        wasConnected = false;

        wifiDisconnectCount++;

        wifiConnected = false;
        ipAddress = "";
        wifiRSSI = 0;

        Serial.println();
        Serial.println("==============================");
        Serial.println("Wi-Fi Connection Lost");
        Serial.println("==============================");

        Serial.print("Wi-Fi disconnect count: ");
        Serial.println(wifiDisconnectCount);
    }
    else
    {
        wifiConnected = false;
        ipAddress = "";
        wifiRSSI = 0;
    }


    // =================================================
    // Finish an expired reconnect attempt
    // =================================================

    if (reconnectPending &&
        millis() - reconnectStartTime >= WIFI_RECONNECT_TIMEOUT)
    {
        reconnectPending = false;
        wifiReconnectFailureCount++;

        Serial.print(
            "Wi-Fi Reconnect FAILED | Total failures: "
        );
        Serial.println(
            wifiReconnectFailureCount
        );
    }


    // =================================================
    // No Saved Credentials
    // =================================================

    if (!isWiFiConfigured())
    {
        return;
    }


    // =================================================
    // Retry Pause
    // =================================================

    if (retryPause)
    {
        if (millis() - retryPauseStart < WIFI_RETRY_PAUSE)
        {
            return;
        }

        retryPause = false;
        reconnectAttempts = 0;

        Serial.println();
        Serial.println("==============================");
        Serial.println("Wi-Fi Retry Cycle Restarted");
        Serial.println("==============================");
    }


    // =================================================
    // Wait Between Attempts
    // =================================================

    if (millis() - lastReconnectAttempt < WIFI_RETRY_INTERVAL)
    {
        return;
    }


    // =================================================
    // Maximum Attempts
    // =================================================

    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS)
    {
        Serial.println();
        Serial.println("==============================");
        Serial.println("Wi-Fi Retry Limit Reached");
        Serial.println("==============================");

        Serial.println(
            "Waiting before next retry cycle..."
        );

        retryPause = true;
        retryPauseStart = millis();

        return;
    }


    // =================================================
    // Get Saved Credentials
    // =================================================

    String ssid = getWiFiSSID();
    String password = getWiFiPassword();

    if (ssid.length() == 0)
    {
        Serial.println("No saved Wi-Fi SSID");
        return;
    }


    // =================================================
    // Start Reconnect
    // =================================================

    reconnectAttempts++;
    wifiReconnectAttemptCount++;

    lastReconnectAttempt = millis();
    reconnectStartTime = millis();
    reconnectPending = true;

    Serial.println();
    Serial.println("==============================");

    Serial.print(
        "Wi-Fi Reconnect Attempt "
    );

    Serial.println(
        reconnectAttempts
    );

    Serial.println(
        "=============================="
    );

    WiFi.disconnect();
    delay(100);

    WiFi.begin(
        ssid.c_str(),
        password.c_str()
    );

    Serial.println("Reconnect started");
}


// =====================================================
// Diagnostics
// =====================================================

uint32_t wifiGetDisconnectCount()
{
    return wifiDisconnectCount;
}

uint32_t wifiGetReconnectAttemptCount()
{
    return wifiReconnectAttemptCount;
}

uint32_t wifiGetReconnectSuccessCount()
{
    return wifiReconnectSuccessCount;
}

uint32_t wifiGetReconnectFailureCount()
{
    return wifiReconnectFailureCount;
}


bool consumeWiFiReconnectEvent()
{
    if (!reconnectEventPending)
    {
        return false;
    }

    reconnectEventPending = false;

    return true;
}
