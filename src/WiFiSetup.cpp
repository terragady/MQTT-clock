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

  // An OTA update reboots via a soft reset, which does not fully reinitialise
  // the ESP8266 RF peripheral. The first association attempt after a flash
  // therefore often fails even though the stored credentials are valid, and
  // WiFiManager's default of a single try means it gives up immediately and
  // raises the config portal. setCleanConnect() forces an explicit disconnect
  // first, which clears the stale state; the retries and timeout stop one
  // slow association from dropping us into the portal.
  wifiManager.setCleanConnect(true);
  wifiManager.setConnectRetries(WIFI_CONNECT_RETRIES);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SECONDS);

  if (!wifiManager.autoConnect(WIFI_PORTAL_AP_NAME))
  {
    Serial.println(F("Config portal timed out, rebooting to retry WiFi..."));
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  Serial.println(F("WiFi connected successfully"));
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

  Serial.println(F("WiFi connection lost, attempting to reconnect..."));
  reconnectAttempts++;

  if (reconnectAttempts > WIFI_MAX_RECONNECT_ATTEMPTS)
  {
    Serial.println(F("Max reconnection attempts reached, rebooting..."));
    display.fillScreen(LOW);
    display.centerPrint("Reset"); // "Reboot" is 36 px and gets clipped on the 32 px panel
    delay(1000);
    ESP.restart();
    return false;
  }

  Serial.print(F("Reconnect attempt "));
  Serial.print(reconnectAttempts);
  Serial.print(F("/"));
  Serial.println(WIFI_MAX_RECONNECT_ATTEMPTS);

  // Not WiFi.disconnect() + WiFi.begin(): on the ESP8266 core, disconnect()
  // erases the stored SSID/password (and persists that to flash), so begin()
  // would retry with empty credentials and the next reboot would open the
  // config portal. reconnect() drops and rejoins without touching them.
  WiFi.reconnect();

  // Wait for connection with timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
  {
    delay(500);
    Serial.print(F("."));
    ESP.wdtFeed(); // Feed watchdog during reconnection
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("\nWiFi reconnected!"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
    reconnectAttempts = 0;
    return true;
  }

  Serial.println(F("\nReconnection failed, will retry later"));
  return false;
}

void WiFiSetup::configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println(F("Entered config mode"));
  Serial.println(WiFi.softAPIP());
  Serial.println(F("Wifi Manager"));
  Serial.println(F("Please connect to AP"));
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println(F("To setup Wifi Configuration"));

  if (displayInstance)
  {
    displayInstance->scrollMessage("Please Connect to AP: " + String(myWiFiManager->getConfigPortalSSID()));
    displayInstance->centerPrint("WiFi");
  }
}
