#include "WiFiSetup.h"
#include "Settings.h"
#include <ESP8266WiFi.h>

// Static member initialization
DisplayManager *WiFiSetup::displayInstance = nullptr;

WiFiSetup::WiFiSetup(DisplayManager &displayRef) : display(displayRef), reconnectAttempts(0)
{
  displayInstance = &display; // Set static reference for callback
}

void WiFiSetup::initialize()
{
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  // Give the config portal a generous timeout so a transient router outage
  // doesn't trap the device on the "WiFi" portal screen forever. When it
  // expires, autoConnect() returns false and we reboot to retry the router.
  // Kept long (10 min) so an in-progress manual setup isn't cut short.
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_PORTAL_TIMEOUT_SECONDS);

  if (!wifiManager.autoConnect(WIFI_PORTAL_AP_NAME.c_str()))
  {
    Serial.println("Config portal timed out, rebooting to retry WiFi...");
    delay(3000);
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

bool WiFiSetup::checkConnection()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    reconnectAttempts = 0; // Reset counter on successful connection
    return true;
  }

  Serial.println("WiFi connection lost, attempting to reconnect...");
  reconnectAttempts++;

  if (reconnectAttempts > WIFI_MAX_RECONNECT_ATTEMPTS)
  {
    Serial.println("Max reconnection attempts reached, rebooting...");
    display.fillScreen(LOW);
    display.centerPrint("Reboot");
    delay(1000);
    ESP.restart();
    return false;
  }

  Serial.print("Reconnect attempt ");
  Serial.print(reconnectAttempts);
  Serial.print("/");
  Serial.println(WIFI_MAX_RECONNECT_ATTEMPTS);

  WiFi.disconnect();
  delay(WIFI_RECONNECT_DELAY);
  WiFi.begin(); // Reconnect using stored credentials

  // Wait for connection with timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
  {
    delay(500);
    Serial.print(".");
    ESP.wdtFeed(); // Feed watchdog during reconnection
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi reconnected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    reconnectAttempts = 0;
    return true;
  }

  Serial.println("\nReconnection failed, will retry later");
  return false;
}

bool WiFiSetup::isConnected() const
{
  return WiFi.status() == WL_CONNECTED;
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
