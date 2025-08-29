#pragma once
#include "Arduino.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include "DisplayManager.h"

class WebOTAManager
{
public:
    WebOTAManager(DisplayManager &displayRef);

    // Web OTA operations
    void initialize();
    void loop();
    String getUpdateURL();

private:
    DisplayManager &display;
    ESP8266WebServer httpServer;
    ESP8266HTTPUpdateServer httpUpdater;

    // Web page handlers
    void handleRoot();
    void handleStatus();
    void handleInfo();

    // HTML content
    String getIndexHTML();
    String getStatusHTML();
};
