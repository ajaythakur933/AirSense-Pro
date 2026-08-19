#include "storage.h"
#include <Preferences.h>

Preferences preferences;

//--------------------------------------
// Initialize
//--------------------------------------
bool initStorage()
{
    return preferences.begin("airsense", false);
}

//--------------------------------------
// Check WiFi Configuration
//--------------------------------------
bool isWiFiConfigured()
{
    String ssid = preferences.getString("ssid", "");

    return ssid.length() > 0;
}

//--------------------------------------
// Save Credentials
//--------------------------------------
bool saveWiFiCredentials(const String &ssid, const String &password)
{
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);

    return true;
}

//--------------------------------------
// Read SSID
//--------------------------------------
String getWiFiSSID()
{
    return preferences.getString("ssid", "");
}

//--------------------------------------
// Read Password
//--------------------------------------
String getWiFiPassword()
{
    return preferences.getString("password", "");
}

//--------------------------------------
// Factory Reset
//--------------------------------------
void clearWiFiCredentials()
{
    preferences.clear();
}