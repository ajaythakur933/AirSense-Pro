#include "logger.h"
#include <Wire.h>
#include <WiFi.h>

#include "config.h"
#include "version.h"
#include "globals.h"

#include "storage.h"
#include "sensor.h"
#include "air_quality.h"
#include "display.h"
#include "wifi_manager.h"
#include <ESPmDNS.h>
#include "setup_portal.h"
#include "history.h"


// =====================================================
// SETUP
// =====================================================


// =====================================================
// mDNS
// =====================================================

static bool mdnsReady = false;

static bool initMDNS()
{
    if (MDNS.begin("airsense"))
    {
        MDNS.addService("http", "tcp", 80);
        mdnsReady = true;

        Serial.println();
        Serial.println("==============================");
        Serial.println("mDNS Service");
        Serial.println("==============================");
        Serial.println("Hostname : airsense.local");
        Serial.println("Dashboard: http://airsense.local");
        Serial.println("mDNS Ready");

        return true;
    }

    mdnsReady = false;

    Serial.println("mDNS initialization FAILED");
    return false;
}


// =====================================================
// Refresh mDNS after Wi-Fi recovery
// =====================================================

static void refreshMDNS()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("Refreshing mDNS");
    Serial.println("==============================");

    if (mdnsReady)
    {
        MDNS.end();
        mdnsReady = false;
        delay(20);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("mDNS refresh skipped - Wi-Fi offline");
        return;
    }

    if (MDNS.begin("airsense"))
    {
        MDNS.addService("http", "tcp", 80);
        mdnsReady = true;

        Serial.println("mDNS refresh SUCCESS");
        Serial.println("Hostname : airsense.local");
    }
    else
    {
        Serial.println("mDNS refresh FAILED");
    }
}

void setup()
{
    Serial.begin(115200);

    delay(1000);


    // =================================================
    // I2C
    // =================================================

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );


    // =================================================
    // Boot Information
    // =================================================

    Serial.println();

    Serial.println(
        "===================================="
    );

    Serial.println(
        PRODUCT_NAME
    );

    Serial.print(
        "Firmware : "
    );

    Serial.println(
        PRODUCT_VERSION
    );

    Serial.println(
        "===================================="
    );


    // =================================================
    // Storage
    // =================================================

    if (!initStorage())
    {
        Serial.println(
            "Storage Initialization Failed!"
        );

        while (1);
    }

    Serial.println(
        "Storage Initialized"
    );

    if (!initHistory())
    {
        Serial.println(
            "History Storage Initialization Failed"
        );
    }


    // =================================================
    // Display
    // =================================================

    if (!initDisplay())
    {
        Serial.println(
            "OLED Initialization Failed!"
        );

        while (1);
    }

    Serial.println(
        "Display Initialized"
    );


    // =================================================
    // Sensor
    // =================================================

    if (!initSensor())
    {
        Serial.println(
            "BME680 Initialization Failed!"
        );

        while (1);
    }

    Serial.println(
        "Sensor Initialized"
    );

    // =================================================
    // Air Quality
    // =================================================

    initAirQuality();


    // =================================================
    // Wi-Fi
    // =================================================

    if (!initWiFi())
    {
        Serial.println(
            "WiFi Initialization Failed!"
        );
    }
    else
    {
        // ---------------------------------------------
        // Saved Wi-Fi Available
        // ---------------------------------------------

        if (isWiFiConfigured())
        {
            Serial.println(
                "WiFi Credentials Found"
            );


            // -----------------------------------------
            // Connect to Saved Wi-Fi
            // -----------------------------------------

            if (connectToSavedWiFi())
            {
                // =====================================
                // Start Web Server
                // =====================================

                initMDNS();

                if (initWebServer())
                {
                    Serial.println(
                        "Web Server Started"
                    );
                }
                else
                {
                    Serial.println(
                        "Web Server Failed"
                    );
                }
            }


            // -----------------------------------------
            // Saved Wi-Fi Connection Failed
            // -----------------------------------------

            else
            {
                Serial.println(
                    "Saved WiFi Connection Failed"
                );

                Serial.println(
                    "Starting Setup Portal"
                );


                if (initSetupPortal())
                {
                    Serial.println(
                        "Setup Portal Started"
                    );
                }
                else
                {
                    Serial.println(
                        "Setup Portal Failed"
                    );
                }
            }
        }


        // ---------------------------------------------
        // No Saved Wi-Fi
        // ---------------------------------------------

        else
        {
            Serial.println(
                "No WiFi Saved"
            );

            Serial.println(
                "Starting Setup Portal"
            );


            if (initSetupPortal())
            {
                Serial.println(
                    "Setup Portal Started"
                );
            }
            else
            {
                Serial.println(
                    "Setup Portal Failed"
                );
            }
        }
    }


    // =================================================
    // System Ready
    // =================================================

    Serial.println();

    Serial.println(
        "===== System Ready ====="
    );
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  static bool tcpLoggerStarted = false;
  if (!tcpLoggerStarted && WiFi.status() == WL_CONNECTED) {
    loggerBegin();
    tcpLoggerStarted = true;
  }
  loggerLoop();

    // =================================================
    // Sensor
    // =================================================

    // Update environmental sensor and calculate air quality
    // only when a fresh valid BME680 reading is available.
    if (updateSensor())
    {
        AirQualityData airQuality =
            calculateAirQuality(
                gasResistance,
                humidity,
                temperature
            );

        loggerPrint("Air Quality | Score: ");
        loggerPrint(String(airQuality.score, 1));
        loggerPrint("/100 | Status: ");
        loggerPrintln(airQuality.level);

        // Store one historical sample every five minutes. The history module
        // handles RAM buffering and periodic NVS persistence.
        historyRecord(
            temperature,
            humidity,
            pressure,
            gasResistance,
            airQuality.score
        );
    }


    // =================================================
    // Display
    // =================================================

    updateDisplay();


    // =================================================
    // Wi-Fi
    // =================================================

    updateWiFi();

    // V1.5.0-B:
    // If a previously connected Wi-Fi link has recovered,
    // refresh mDNS so airsense.local is registered again
    // against the current network interface/IP.
    if (consumeWiFiReconnectEvent())
    {
        refreshMDNS();
    }


    // =================================================
    // Web Server / Setup Portal
    // =================================================

    updateSetupPortal();


    // =================================================
    // Historical data maintenance
    // =================================================

    historyLoop();

    // =================================================
    // Small Loop Delay
    // =================================================

    delay(100);
}