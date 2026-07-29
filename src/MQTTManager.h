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

// Timing constants
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // Time between reconnect attempts (ms)
const int MQTT_MAX_RECONNECT_ATTEMPTS = 3;          // Max attempts before yielding to loop
const int MQTT_BUFFER_SIZE = 1024;                  // MQTT buffer size for discovery messages
const int MQTT_CONNECT_TIMEOUT_MS = 3000;           // Max blocking time for TCP connect (ms)
const uint16_t MQTT_SOCKET_TIMEOUT_S = 3;           // Max blocking time for MQTT handshake/reads (s)
const unsigned long MQTT_STATUS_PUBLISH_INTERVAL = 60000UL; // Periodic status refresh (ms)

class MQTTManager
{
public:
  MQTTManager(DisplayManager &displayRef, TimeManager &timeRef);

  // MQTT operations
  void initialize();
  void loop();
  bool tryReconnect(); // Non-blocking reconnect attempt
  bool isConnected();
  bool isFilesystemAvailable() const { return filesystemAvailable; }

  // Message handling
  void sendStatus(const String &status);
  void sendDiscoveryConfig();
  void publishNotificationHelp(); // Retained usage docs shown as HA attributes

  // Brightness management
  void setDayBrightness(int brightness);
  void setNightBrightness(int brightness);
  void setDayStartMinutes(int minutes);
  void setNightStartMinutes(int minutes);
  void updateBrightnessBasedOnTime();

  // Getters for current settings
  int getDayBrightness() const { return dayBrightness; }
  int getNightBrightness() const { return nightBrightness; }
  int getDayStartMinutes() const { return dayStartMinutes; }
  int getNightStartMinutes() const { return nightStartMinutes; }
  bool isShowingNotification() const { return showingNotification; }

private:
  DisplayManager &display;
  TimeManager &timeManager;
  WiFiClient wifiClient;
  PubSubClient mqttClient;

  // Brightness settings
  int dayBrightness;
  int nightBrightness;
  // Schedule stored as minutes-since-midnight (0-1439) for HH:MM granularity.
  int dayStartMinutes;
  int nightStartMinutes;

  // Notification handling
  String currentNotification;
  bool showingNotification;
  std::queue<NotificationConfig> notificationQueue;
  NotificationConfig currentConfig;

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

  // Time-of-day helpers for HH:MM schedule handling
  static String minutesToTimeString(int minutes);      // e.g. 420 -> "07:00:00"
  static int parseTimeStringToMinutes(const String &value); // "HH:MM[:SS]" -> minutes, -1 if invalid

  // Reconnection tracking
  unsigned long lastReconnectAttempt;
  int reconnectAttempts;

  // Periodic status refresh so HA sensors (e.g. Day/Night mode) stay current
  // even when no command arrives.
  unsigned long lastStatusPublish;

  // Filesystem status
  bool filesystemAvailable;

  // SPIFFS storage functions
  void loadSettings();
  void saveSettings();

  // Static reference for callback
  static MQTTManager *instance;
};
