#pragma once
#include "Arduino.h"
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include "DisplayManager.h"

class OTAManager
{
public:
  OTAManager(DisplayManager &displayRef);

  // OTA operations
  void initialize();
  void loop();

private:
  DisplayManager &display;

  // OTA event handlers
  static void onStart();
  static void onEnd();
  static void onProgress(unsigned int progress, unsigned int total);
  static void onError(ota_error_t error);

  // Static reference for callbacks
  static OTAManager *instance;
};
