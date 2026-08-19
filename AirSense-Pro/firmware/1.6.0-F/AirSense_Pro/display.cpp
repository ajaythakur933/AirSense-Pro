#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WiFi.h>

#include "display.h"
#include "config.h"
#include "version.h"
#include "globals.h"
#include "air_quality.h"

// -------------------------------------------------
// OLED Object
// -------------------------------------------------
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------------------------------
// Local Variables
// -------------------------------------------------
static unsigned long lastPageChange = 0;

// -------------------------------------------------
// Initialize Display
// -------------------------------------------------
bool initDisplay()
{
    if (!display.begin(OLED_ADDR, true))
    {
        return false;
    }

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);

    drawBootScreen();

    lastPageChange = millis();
    currentPage = 0;

    return true;
}

// -------------------------------------------------
// Boot Screen
// -------------------------------------------------
void drawBootScreen()
{
    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(8,15);
    display.println(PRODUCT_NAME);

    display.setTextSize(1);
    display.setCursor(28,45);
    display.println("Starting...");

    display.display();

    delay(2000);
}

// -------------------------------------------------
// Status Bar
// -------------------------------------------------
void drawStatusBar()
{
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    // Compact Wi-Fi state indicator.
    display.setCursor(0,0);

    if (WiFi.status() == WL_CONNECTED)
    {
        display.print("WiFi OK");
    }
    else
    {
        display.print("WiFi --");
    }

    display.setCursor(105,0);
    display.print(currentPage + 1);
    display.print("/");
    display.print(PAGE_COUNT);

    display.drawLine(0,10,127,10,SH110X_WHITE);
}

// -------------------------------------------------
// Page 1 - Air Quality
// -------------------------------------------------
void drawHomePage()
{
    AirQualityData airQuality =
        calculateAirQuality(
            gasResistance,
            humidity,
            temperature
        );

    display.setTextSize(1);
    display.setCursor(0,15);
    display.print("AIR QUALITY");

    display.setTextSize(2);
    display.setCursor(0,27);
    display.print(airQuality.score,1);
    display.print("/100");

    display.setTextSize(1);
    display.setCursor(0,50);
    display.print(airQuality.level);

    display.setCursor(82,50);
    display.print("Gas ");
    display.print(gasResistance,0);
    display.print("K");
}

// -------------------------------------------------
// Page 2 - Environmental
// -------------------------------------------------
void drawEnvironmentPage()
{
    display.setTextSize(1);

    display.setCursor(0,15);
    display.print("ENVIRONMENT");

    display.setCursor(0,27);
    display.print("Temp     ");
    display.print(temperature,1);
    display.print(" C");

    display.setCursor(0,38);
    display.print("Humidity ");
    display.print(humidity,1);
    display.print(" %");

    display.setCursor(0,49);
    display.print("Pressure ");
    display.print(pressure,1);
    display.print(" hPa");
}

// -------------------------------------------------
// Page 3 - Network
// -------------------------------------------------
void drawNetworkPage()
{
    display.setTextSize(1);

    display.setCursor(0,15);
    display.print("NETWORK");

    if (WiFi.status() == WL_CONNECTED)
    {
        display.setCursor(0,26);
        display.print("WiFi: CONNECTED");

        display.setCursor(0,37);
        display.print("IP ADDRESS");

        String ip = WiFi.localIP().toString();

        display.setCursor(0,48);
        display.print(ip);

        display.setCursor(87,48);
        display.print(WiFi.RSSI());
        display.print("dB");
    }
    else
    {
        display.setCursor(0,28);
        display.print("WiFi: DISCONNECTED");

        display.setCursor(0,42);
        display.print("Dashboard unavailable");
    }
}

// -------------------------------------------------
// Page 4 - System
// -------------------------------------------------
void drawSystemPage()
{
    display.setTextSize(1);

    display.setCursor(0,15);
    display.println(PRODUCT_NAME);

    display.setCursor(0,28);
    display.print("Firmware : ");
    display.println(PRODUCT_VERSION);

    display.setCursor(0,40);
    display.print("Sensor   : OK");

    display.setCursor(0,52);

    if (WiFi.status() == WL_CONNECTED)
    {
        display.print("Network  : ONLINE");
    }
    else
    {
        display.print("Network  : OFFLINE");
    }
}

// -------------------------------------------------
// Update Display
// -------------------------------------------------
void updateDisplay()
{
    if (millis() - lastPageChange >= PAGE_CHANGE_TIME)
    {
        lastPageChange = millis();

        currentPage++;

        if (currentPage >= PAGE_COUNT)
        {
            currentPage = 0;
        }
    }

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);

    drawStatusBar();

    switch(currentPage)
    {
        case 0:
            drawHomePage();
            break;

        case 1:
            drawEnvironmentPage();
            break;

        case 2:
            drawNetworkPage();
            break;

        case 3:
            drawSystemPage();
            break;

        default:
            currentPage = 0;
            drawHomePage();
            break;
    }

    display.display();
}
