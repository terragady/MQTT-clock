#include "MQTTManager.h"
#include "Settings.h"
#include <TimeLib.h>

// Static member initialization
MQTTManager *MQTTManager::instance = nullptr;

MQTTManager::MQTTManager(DisplayManager &displayRef, TimeManager &timeRef)
    : display(displayRef), timeManager(timeRef), mqttClient(wifiClient),
      dayBrightness(DEFAULT_DAY_BRIGHTNESS), nightBrightness(DEFAULT_NIGHT_BRIGHTNESS),
      dayStartHour(DEFAULT_DAY_START_HOUR), nightStartHour(DEFAULT_NIGHT_START_HOUR),
      showingNotification(false), originalBrightness(-1)
{
  instance = this; // Set static reference for callback
}

void MQTTManager::initialize()
{
  // Initialize SPIFFS first
  if (!LittleFS.begin())
  {
    Serial.println("SPIFFS initialization failed! Using defaults.");
  }
  else
  {
    Serial.println("SPIFFS initialized successfully");
    loadSettings(); // Load saved settings from SPIFFS
  }

  mqttClient.setServer(MQTT_SERVER.c_str(), MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024); // Increased buffer size for discovery messages

  Serial.println("MQTT Manager initialized");

  reconnect();
}

void MQTTManager::loop()
{
  if (!mqttClient.connected())
  {
    reconnect();
  }
  mqttClient.loop();

  // Update brightness based on current time (only if not showing notification)
  if (!showingNotification)
  {
    updateBrightnessBasedOnTime();
  }
}

void MQTTManager::reconnect()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");

    String clientId = MQTT_CLIENT_ID + "-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USER.c_str(), MQTT_PASSWORD.c_str(),
                           MQTT_TOPIC_STATUS.c_str(), 0, true, "{\"status\":\"offline\"}"))
    {
      Serial.println(" connected!");

      // Subscribe to topics
      mqttClient.subscribe(MQTT_TOPIC_NOTIFICATION.c_str());
      mqttClient.subscribe(MQTT_TOPIC_BRIGHTNESS_DAY.c_str());
      mqttClient.subscribe(MQTT_TOPIC_BRIGHTNESS_NIGHT.c_str());
      mqttClient.subscribe(MQTT_TOPIC_SCHEDULE_DAY_START.c_str());
      mqttClient.subscribe(MQTT_TOPIC_SCHEDULE_NIGHT_START.c_str());
      mqttClient.subscribe((MQTT_TOPIC_PREFIX + "/discovery").c_str());

      // Send discovery config and status
      sendDiscoveryConfig();
      sendStatus("online");

      Serial.println("Subscribed to MQTT topics");
    }
    else
    {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

bool MQTTManager::isConnected()
{
  return mqttClient.connected();
}

void MQTTManager::sendStatus(const String &status)
{
  if (mqttClient.connected())
  {
    String payload = "{";
    payload += "\"status\":\"" + status + "\",";
    payload += "\"day_brightness\":" + String(dayBrightness) + ",";
    payload += "\"night_brightness\":" + String(nightBrightness) + ",";
    payload += "\"day_start_hour\":" + String(dayStartHour) + ",";
    payload += "\"night_start_hour\":" + String(nightStartHour) + ",";
    payload += "\"current_time\":\"" + timeManager.getFormattedTime(true) + "\",";
    payload += "\"is_day_time\":" + String(isDayTime() ? "true" : "false");
    payload += "}";

    mqttClient.publish(MQTT_TOPIC_STATUS.c_str(), payload.c_str(), true); // Retained status
  }
}

void MQTTManager::sendDiscoveryConfig()
{
  if (!mqttClient.connected())
  {
    return;
  }

  String device_id = "mqtt_clock_" + WiFi.macAddress();
  device_id.replace(":", "");

  // Device information shared across all entities
  String device_info = "\"device\":{"
                       "\"identifiers\":[\"" +
                       device_id + "\"],"
                                   "\"name\":\"MQTT Clock\","
                                   "\"model\":\"ESP8266 LED Matrix Clock\","
                                   "\"manufacturer\":\"Custom\","
                                   "\"sw_version\":\"1.0\""
                                   "}";

  // 1. Status Sensor
  String status_config = "{"
                         "\"name\":\"Clock Status\","
                         "\"unique_id\":\"" +
                         device_id + "_status\","
                                     "\"state_topic\":\"" +
                         MQTT_TOPIC_STATUS + "\","
                                             "\"value_template\":\"{{ value_json.status }}\","
                                             "\"icon\":\"mdi:clock-digital\"," +
                         device_info + "}";

  mqttClient.publish("homeassistant/sensor/mqtt_clock/status/config", status_config.c_str(), true);

  // 2. Current Time Sensor
  String time_config = "{"
                       "\"name\":\"Current Time\","
                       "\"unique_id\":\"" +
                       device_id + "_time\","
                                   "\"state_topic\":\"" +
                       MQTT_TOPIC_STATUS + "\","
                                           "\"value_template\":\"{{ value_json.current_time }}\","
                                           "\"icon\":\"mdi:clock\"," +
                       device_info + "}";

  mqttClient.publish("homeassistant/sensor/mqtt_clock/time/config", time_config.c_str(), true);

  // 3. Day/Night Mode Sensor
  String daynight_config = "{"
                           "\"name\":\"Day/Night Mode\","
                           "\"unique_id\":\"" +
                           device_id + "_daynight\","
                                       "\"state_topic\":\"" +
                           MQTT_TOPIC_STATUS + "\","
                                               "\"value_template\":\"{% if value_json.is_day_time %}Day{% else %}Night{% endif %}\","
                                               "\"icon\":\"mdi:weather-sunny\"," +
                           device_info + "}";

  mqttClient.publish("homeassistant/sensor/mqtt_clock/daynight/config", daynight_config.c_str(), true);

  // 4. Day Brightness Number Control
  String day_brightness_config = "{"
                                 "\"name\":\"Day Brightness\","
                                 "\"unique_id\":\"" +
                                 device_id + "_day_brightness\","
                                             "\"state_topic\":\"" +
                                 MQTT_TOPIC_STATUS + "\","
                                                     "\"command_topic\":\"" +
                                 MQTT_TOPIC_BRIGHTNESS_DAY + "\","
                                                             "\"value_template\":\"{{ value_json.day_brightness }}\","
                                                             "\"min\":0,\"max\":15,\"step\":1,"
                                                             "\"icon\":\"mdi:brightness-6\"," +
                                 device_info + "}";

  mqttClient.publish("homeassistant/number/mqtt_clock/day_brightness/config", day_brightness_config.c_str(), true);

  // 5. Night Brightness Number Control
  String night_brightness_config = "{"
                                   "\"name\":\"Night Brightness\","
                                   "\"unique_id\":\"" +
                                   device_id + "_night_brightness\","
                                               "\"state_topic\":\"" +
                                   MQTT_TOPIC_STATUS + "\","
                                                       "\"command_topic\":\"" +
                                   MQTT_TOPIC_BRIGHTNESS_NIGHT + "\","
                                                                 "\"value_template\":\"{{ value_json.night_brightness }}\","
                                                                 "\"min\":0,\"max\":15,\"step\":1,"
                                                                 "\"icon\":\"mdi:brightness-3\"," +
                                   device_info + "}";

  mqttClient.publish("homeassistant/number/mqtt_clock/night_brightness/config", night_brightness_config.c_str(), true);

  // 6. Notification Text Input
  String notification_config = "{"
                               "\"name\":\"Send Notification\","
                               "\"unique_id\":\"" +
                               device_id + "_notification\","
                                           "\"command_topic\":\"" +
                               MQTT_TOPIC_NOTIFICATION + "\","
                                                         "\"icon\":\"mdi:message-text\"," +
                               device_info + "}";

  mqttClient.publish("homeassistant/text/mqtt_clock/notification/config", notification_config.c_str(), true);
}

void MQTTManager::mqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (instance)
  {
    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
      message += (char)payload[i];
    }
    instance->handleMessage(String(topic), message);
  }
}

void MQTTManager::handleMessage(const String &topic, const String &message)
{
  if (topic == MQTT_TOPIC_NOTIFICATION)
  {
    // Check if message is JSON (starts with '{')
    if (message.startsWith("{"))
    {
      parseNotificationJson(message);
    }
    else
    {
      showNotification(message);
    }
  }
  else if (topic == MQTT_TOPIC_BRIGHTNESS_DAY)
  {
    int brightness = message.toInt();
    if (brightness >= 0 && brightness <= 15)
    {
      setDayBrightness(brightness);
    }
  }
  else if (topic == MQTT_TOPIC_BRIGHTNESS_NIGHT)
  {
    int brightness = message.toInt();
    if (brightness >= 0 && brightness <= 15)
    {
      setNightBrightness(brightness);
    }
  }
  else if (topic == MQTT_TOPIC_SCHEDULE_DAY_START)
  {
    int hour = message.toInt();
    if (hour >= 0 && hour <= 23)
    {
      setDayStartHour(hour);
    }
  }
  else if (topic == MQTT_TOPIC_SCHEDULE_NIGHT_START)
  {
    int hour = message.toInt();
    if (hour >= 0 && hour <= 23)
    {
      setNightStartHour(hour);
    }
  }
  else if (topic == MQTT_TOPIC_PREFIX + "/discovery")
  {
    sendDiscoveryConfig();
  }

  // Send updated status
  sendStatus("online");
}

void MQTTManager::showNotification(const String &message)
{
  currentNotification = message;
  showingNotification = true;

  Serial.println("Showing notification: " + message);
  display.scrollMessage(message);

  // Reset notification flag after scroll completes
  showingNotification = false;
  Serial.println("Notification display completed - returning to clock display");
}

void MQTTManager::setDayBrightness(int brightness)
{
  dayBrightness = constrain(brightness, 0, 15);
  updateBrightnessBasedOnTime();
  saveSettings(); // Save to SPIFFS
}

void MQTTManager::setNightBrightness(int brightness)
{
  nightBrightness = constrain(brightness, 0, 15);
  updateBrightnessBasedOnTime();
  saveSettings(); // Save to SPIFFS
}

void MQTTManager::setDayStartHour(int hour)
{
  dayStartHour = constrain(hour, 0, 23);
  updateBrightnessBasedOnTime();
  saveSettings(); // Save to SPIFFS
}

void MQTTManager::setNightStartHour(int hour)
{
  nightStartHour = constrain(hour, 0, 23);
  updateBrightnessBasedOnTime();
  saveSettings(); // Save to SPIFFS
}

void MQTTManager::updateBrightnessBasedOnTime()
{
  int targetBrightness = isDayTime() ? dayBrightness : nightBrightness;
  display.setIntensity(targetBrightness);
}

bool MQTTManager::isDayTime()
{
  int currentHour = hour();

  // Handle normal case (day start < night start)
  if (dayStartHour < nightStartHour)
  {
    return (currentHour >= dayStartHour && currentHour < nightStartHour);
  }
  // Handle wrap-around case (night start < day start, e.g., day starts at 6, night starts at 22)
  else
  {
    return (currentHour >= dayStartHour || currentHour < nightStartHour);
  }
}

void MQTTManager::loadSettings()
{
  File file = LittleFS.open("/clock_settings.json", "r");
  if (!file)
  {
    Serial.println("No settings file found, using defaults");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.println("Failed to parse settings file, using defaults");
    return;
  }

  // Load settings with defaults if not found
  dayBrightness = doc["day_brightness"] | DEFAULT_DAY_BRIGHTNESS;
  nightBrightness = doc["night_brightness"] | DEFAULT_NIGHT_BRIGHTNESS;
  dayStartHour = doc["day_start_hour"] | DEFAULT_DAY_START_HOUR;
  nightStartHour = doc["night_start_hour"] | DEFAULT_NIGHT_START_HOUR;
}

void MQTTManager::saveSettings()
{
  JsonDocument doc;

  doc["day_brightness"] = dayBrightness;
  doc["night_brightness"] = nightBrightness;
  doc["day_start_hour"] = dayStartHour;
  doc["night_start_hour"] = nightStartHour;

  File file = LittleFS.open("/clock_settings.json", "w");
  if (!file)
  {
    Serial.println("Failed to open settings file for writing");
    return;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write settings to file");
  }

  file.close();
}

void MQTTManager::showAdvancedNotification(const NotificationConfig &config)
{
  currentConfig = config;
  currentNotification = config.message;
  showingNotification = true;

  // Store original brightness if we need to change it
  if (config.brightness >= 0)
  {
    originalBrightness = isDayTime() ? dayBrightness : nightBrightness;
    display.setIntensity(config.brightness);
  }

  Serial.println("Showing advanced notification: " + config.message);
  Serial.println("Scrolling: " + String(config.isScrolling ? "Yes" : "No"));
  Serial.println("Repeats: " + String(config.scrollRepeat));
  Serial.println("Speed: " + String(config.scrollSpeed) + "ms");
  Serial.println("Brightness: " + String(config.brightness));
  Serial.println("Flash effect: " + String(config.flashEffect ? "Yes" : "No"));
  if (config.flashEffect)
  {
    Serial.println("Flash count: " + String(config.flashCount));
  }

  if (config.isScrolling)
  {
    // For scrolling notifications, flash effect is not supported
    if (config.flashEffect)
    {
      Serial.println("Warning: Flash effect not supported for scrolling notifications");
    }

    // Perform all scrolling repeats (blocking)
    Serial.println("Scrolling notification " + String(config.scrollRepeat) + " times");
    for (int i = 0; i < config.scrollRepeat; i++)
    {
      Serial.println("Scroll repeat " + String(i + 1) + " of " + String(config.scrollRepeat));
      display.scrollMessage(config.message, config.scrollSpeed);
      if (i < config.scrollRepeat - 1) // Small delay between repeats (except last one)
      {
        delay(500);
      }
    }

    // All scrolling done, return to clock
    showingNotification = false;
    if (originalBrightness >= 0)
    {
      display.setIntensity(originalBrightness);
      originalBrightness = -1;
    }
    Serial.println("All scrolling completed - returning to clock display");
  }
  else
  {
    // Static notification
    display.fillScreen(LOW);
    display.centerPrint(config.message);

    // If flash effect is requested, perform brightness animation multiple times
    if (config.flashEffect)
    {
      Serial.println("Performing brightness animation " + String(config.flashCount) + " times");
      for (int i = 0; i < config.flashCount; i++)
      {
        display.performBrightnessAnimation();
      }

      // Restore original brightness after all animations
      if (originalBrightness >= 0)
      {
        display.setIntensity(originalBrightness);
      }
    }
    else
    {
      // No flash effect - add delay to show the message
      Serial.println("Static notification without flash - showing for 3 seconds");
      delay(3000); // Show for 3 seconds
    }

    // Static notification is done (blocking), return to clock
    showingNotification = false;
    if (originalBrightness >= 0)
    {
      display.setIntensity(originalBrightness);
      originalBrightness = -1;
    }
    Serial.println("Static notification completed - returning to clock display");
  }
}

void MQTTManager::parseNotificationJson(const String &jsonString)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error)
  {
    Serial.println("Failed to parse notification JSON, using simple message");
    showNotification(jsonString);
    return;
  }

  NotificationConfig config;

  // Required field
  config.message = doc["message"] | "No message";

  // Optional fields with defaults
  config.isScrolling = doc["scrolling"] | true;
  config.scrollRepeat = constrain(doc["repeat"] | 1, 1, 10);
  config.scrollSpeed = constrain(doc["speed"] | 35, 5, 100);
  config.brightness = constrain(doc["brightness"] | -1, -1, 15);
  config.flashEffect = doc["flash"] | false;
  config.flashCount = constrain(doc["flash_count"] | 3, 1, 10);

  showAdvancedNotification(config);
}
