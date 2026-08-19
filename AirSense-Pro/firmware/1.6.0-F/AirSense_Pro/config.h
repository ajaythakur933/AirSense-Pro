#ifndef CONFIG_H
#define CONFIG_H

/**************************************************
 * AirSense Pro
 * Version 1.6.0-F
 **************************************************/

#define FW_VERSION "1.6.0-F"

//=============================
// OLED
//=============================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDR 0x3C

//=============================
// I2C
//=============================

#define SDA_PIN 13
#define SCL_PIN 14

//=============================
// WiFi
//=============================

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

//=============================
// Display Page Settings
//=============================

// 0 = Air Quality
// 1 = Environmental
// 2 = Network
// 3 = System
#define PAGE_COUNT 4
#define PAGE_CHANGE_TIME 5000UL

//=============================
// Sensor Refresh
//=============================

#define SENSOR_REFRESH 1000UL

#endif
