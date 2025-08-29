#pragma once
#include "Arduino.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <queue>
#include "DisplayManager.h"
#include "TimeManager.h"

// Notification configuration structure
struct NotificationConfig
{
  String message;
  bool isScrolling = true;      // true = scroll, false = static
  int scrollRepeat = 1;         // how many times to scroll (1-10)
  int scrollSpeed = 35;         // scroll speed in ms (5-100)
  int brightness = -1;          // notification brightness (-1 = use current, 0-15)
  bool flashEffect = false;     // flash brightness effect (only for static)
  int flashCount = 3;           // number of flashes (1-10)
  bool isSimpleMessage = false; // true if this is a simple string message
};

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
  bool showingNotification;
  std::queue<NotificationConfig> notificationQueue;

  // Advanced notification state
  NotificationConfig currentConfig;
  int originalBrightness;

  // Helper functions
  static void mqttCallback(char *topic, byte *payload, unsigned int length);
  void handleMessage(const String &topic, const String &message);
  void showNotification(const String &message);
  void showAdvancedNotification(const NotificationConfig &config);
  void parseNotificationJson(const String &jsonString);
  void processNotificationQueue();
  void queueNotification(const NotificationConfig &config);
  void playAnimation(const String &animationType);
  bool isDayTime();

  // SPIFFS storage functions
  void loadSettings();
  void saveSettings();

  // Static reference for callback
  static MQTTManager *instance;
};
