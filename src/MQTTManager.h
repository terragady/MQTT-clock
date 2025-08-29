#pragma once
#include "Arduino.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "DisplayManager.h"
#include "TimeManager.h"

class MQTTManager
{
public:
  MQTTManager(DisplayManager &displayRef, TimeManager &timeRef);

  // MQTT operations
  void initialize();
  void loop();
  void reconnect();
  bool isConnected();

  // Message handling
  void sendStatus(const String &status);
  void sendDiscoveryConfig();

  // Brightness management
  void setDayBrightness(int brightness);
  void setNightBrightness(int brightness);
  void setDayStartHour(int hour);
  void setNightStartHour(int hour);
  void updateBrightnessBasedOnTime();

  // Getters for current settings
  int getDayBrightness() const { return dayBrightness; }
  int getNightBrightness() const { return nightBrightness; }
  int getDayStartHour() const { return dayStartHour; }
  int getNightStartHour() const { return nightStartHour; }
  bool isShowingNotification() const { return showingNotification; }

private:
  DisplayManager &display;
  TimeManager &timeManager;
  WiFiClient wifiClient;
  PubSubClient mqttClient;

  // Brightness settings
  int dayBrightness;
  int nightBrightness;
  int dayStartHour;
  int nightStartHour;

  // Notification handling
  String currentNotification;
  unsigned long notificationStartTime;
  bool showingNotification;

  // Helper functions
  static void mqttCallback(char *topic, byte *payload, unsigned int length);
  void handleMessage(const String &topic, const String &message);
  void showNotification(const String &message);
  bool isDayTime();

  // SPIFFS storage functions
  void loadSettings();
  void saveSettings();

  // Static reference for callback
  static MQTTManager *instance;
};
