#include "WiFiSetup.h"
#include "Settings.h"
#include <ESP8266WiFi.h>

// Static member initialization
DisplayManager *WiFiSetup::displayInstance = nullptr;

WiFiSetup::WiFiSetup(DisplayManager &displayRef) : display(displayRef)
{
  displayInstance = &display; // Set static reference for callback
}

void WiFiSetup::initialize()
{
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  String hostname = "Zegar TV";
  if (!wifiManager.autoConnect(hostname.c_str()))
  {
    Serial.println("Failed to connect to WiFi, restarting...");
    delay(3000);
    WiFi.disconnect(true);
    ESP.reset();
    delay(5000);
  }
  Serial.println("WiFi connected successfully");
}

void WiFiSetup::setHostname(const String &hostname)
{
  wifi_station_set_hostname(hostname.c_str());
  WiFi.hostname(hostname);
}

void WiFiSetup::configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println("Wifi Manager");
  Serial.println("Please connect to AP");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println("To setup Wifi Configuration");

  if (displayInstance)
  {
    displayInstance->scrollMessage("Please Connect to AP: " + String(myWiFiManager->getConfigPortalSSID()));
    displayInstance->centerPrint("WiFi");
  }
}
