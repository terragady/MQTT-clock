#pragma once
#include "Arduino.h"
#include <WiFiManager.h>
#include "DisplayManager.h"

// WiFi reliability constants
const int WIFI_MAX_RECONNECT_ATTEMPTS = 10;     // Max reconnect attempts before reboot
const unsigned long WIFI_RECONNECT_DELAY = 500; // Delay between reconnect attempts (ms)
const int WIFI_CONFIG_PORTAL_TIMEOUT_SECONDS = 600; // Portal timeout before reboot/retry (10 min)

class WiFiSetup
{
public:
  WiFiSetup(DisplayManager &displayRef);

  // WiFi setup and management
  void initialize();
  void setHostname(const String &hostname);
  bool checkConnection(); // Check and reconnect if needed
  bool isConnected() const;

  // Callback for WiFi configuration mode
  static void configModeCallback(WiFiManager *myWiFiManager);

private:
  DisplayManager &display;
  int reconnectAttempts;
  static DisplayManager *displayInstance; // Static reference for callback
};
