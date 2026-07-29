#pragma once

#include <WiFiManager.h> // --> https://github.com/tzapu/WiFiManager
#include "FS.h"
#include <SPI.h>
#include <Adafruit_GFX.h> // --> https://github.com/adafruit/Adafruit-GFX-Library
#include <Max72xxPanel.h> // --> https://github.com/markruys/arduino-Max72xxPanel
#include <pgmspace.h>
#include "TimeDB.h"
#include "secrets.h" // Local, git-ignored credentials (see secrets.example.h)

//******************************
// Start Settings
//******************************

// Time and Display Settings
const int MINUTES_BETWEEN_DATA_REFRESH = 60; // Time in minutes between data refresh
const int DISPLAY_SCROLL_SPEED = 35;         // In milliseconds (slow = 35, normal = 25, fast = 15, very fast = 5)
const bool FLASH_ON_SECONDS = true;          // when true the : character in the time will flash on and off as a seconds indicator

// API Configuration
const String TIMEZONE_DB_API_KEY = SECRET_TIMEZONE_DB_API_KEY;
const String TIMEZONE = "Europe/Oslo";
// Display Hardware Settings
// CLK -> D5 (SCK)
// CS  -> D6
// DIN -> D7 (MOSI)
const int PIN_CS = D6;                       // Attach CS to this pin, DIN to MOSI and CLK to SCK
const int DISPLAY_INTENSITY = 1;             // Brightness (0-15)
const int NUMBER_OF_HORIZONTAL_DISPLAYS = 4; // default 4 for standard 4 x 1 display Max size of 16
const int NUMBER_OF_VERTICAL_DISPLAYS = 1;   // default 1 for a single row height

/* LED Rotation for Display panels (3 is default)
0: no rotation
1: 90 degrees clockwise
2: 180 degrees
3: 90 degrees counter clockwise (default)
*/
const int LED_ROTATION = 3;

// Network Settings
const String DEVICE_HOSTNAME = "ZegarTV";
// SSID of the WiFiManager configuration portal shown on first boot / when it
// cannot join a known network.
const String WIFI_PORTAL_AP_NAME = "Zegar TV";

// MQTT Settings
// Use the broker's IP rather than a bare hostname: the ESP8266's DNS resolver
// often can't resolve names like "homeassistant" (that relies on mDNS).
// Tip: give Home Assistant a DHCP reservation so this IP stays stable.
const String MQTT_SERVER = "192.168.50.42"; // Home Assistant broker IP
const int MQTT_PORT = 1883;
const String MQTT_USER = SECRET_MQTT_USER;
const String MQTT_PASSWORD = SECRET_MQTT_PASSWORD;
const String MQTT_CLIENT_ID = "mqtt-clock";
const String MQTT_TOPIC_PREFIX = "clock/zegarTV";

// MQTT Topics
const String MQTT_TOPIC_NOTIFICATION = MQTT_TOPIC_PREFIX + "/notification";
const String MQTT_TOPIC_NOTIFICATION_HELP = MQTT_TOPIC_PREFIX + "/notification/help"; // Retained usage docs (HA attributes)
const String MQTT_TOPIC_ANIMATION = MQTT_TOPIC_PREFIX + "/animation";
const String MQTT_TOPIC_BRIGHTNESS_DAY = MQTT_TOPIC_PREFIX + "/brightness/day";
const String MQTT_TOPIC_BRIGHTNESS_NIGHT = MQTT_TOPIC_PREFIX + "/brightness/night";
const String MQTT_TOPIC_SCHEDULE_DAY_START = MQTT_TOPIC_PREFIX + "/schedule/day_start";
const String MQTT_TOPIC_SCHEDULE_NIGHT_START = MQTT_TOPIC_PREFIX + "/schedule/night_start";
const String MQTT_TOPIC_STATUS = MQTT_TOPIC_PREFIX + "/status";

// Brightness Settings
const int DEFAULT_DAY_BRIGHTNESS = 8;   // Default day brightness (0-15)
const int DEFAULT_NIGHT_BRIGHTNESS = 1; // Default night brightness (0-15)
// Day/Night schedule is stored as minutes-since-midnight (0-1439) so it can be
// set to any HH:MM, not just whole hours. Home Assistant exposes these as
// "time" entities that a Sun-based automation can drive automatically.
const int DEFAULT_DAY_START_MINUTES = 7 * 60;    // 07:00
const int DEFAULT_NIGHT_START_MINUTES = 22 * 60; // 22:00
