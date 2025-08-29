#pragma once
#include "Arduino.h"
#include <WiFiManager.h>
#include "DisplayManager.h"

class WiFiSetup
{
public:
  WiFiSetup(DisplayManager &displayRef);

  // WiFi setup and management
  void initialize();
  void setHostname(const String &hostname);

  // Callback for WiFi configuration mode
  static void configModeCallback(WiFiManager *myWiFiManager);

private:
  DisplayManager &display;
  static DisplayManager *displayInstance; // Static reference for callback
};
