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

  // Process notification queue if not currently showing a notification
  if (!showingNotification)
  {
    processNotificationQueue();
  }

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
      mqttClient.subscribe(MQTT_TOPIC_ANIMATION.c_str());
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

  // 2. Day/Night Mode Sensor
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

  // 3. Day Brightness Number Control
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

  // 4. Night Brightness Number Control
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

  // 5. Notification Text Input
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
      // Queue simple string message
      NotificationConfig config;
      config.message = message;
      config.isSimpleMessage = true;
      config.isScrolling = true; // Simple messages always scroll
      queueNotification(config);
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
  else if (topic == MQTT_TOPIC_ANIMATION)
  {
    playAnimation(message);
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

  // Update brightness in case day/night changed during notification
  updateBrightnessBasedOnTime();

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

    // Update brightness in case day/night changed during notification
    updateBrightnessBasedOnTime();

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

      // If flash effect is requested, perform brightness animation multiple times
      if (config.flashEffect)
      {
        Serial.println("Performing brightness animation " + String(config.flashCount) + " times");
        for (int i = 0; i < config.flashCount; i++)
        {
          display.performBrightnessAnimation();
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

      // Update brightness in case day/night changed during notification
      updateBrightnessBasedOnTime();

      Serial.println("Static notification completed - returning to clock display");
    }
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
  config.isSimpleMessage = false;

  queueNotification(config);
}

void MQTTManager::queueNotification(const NotificationConfig &config)
{
  notificationQueue.push(config);
  Serial.println("Notification queued: " + config.message + " (Queue size: " + String(notificationQueue.size()) + ")");
}

void MQTTManager::processNotificationQueue()
{
  if (notificationQueue.empty() || showingNotification)
  {
    return; // No notifications to process or already showing one
  }

  // Get the next notification from the queue
  NotificationConfig config = notificationQueue.front();
  notificationQueue.pop();

  Serial.println("Processing notification from queue: " + config.message + " (Remaining in queue: " + String(notificationQueue.size()) + ")");

  // Process the notification
  if (config.isSimpleMessage)
  {
    showNotification(config.message);
  }
  else
  {
    showAdvancedNotification(config);
  }
}

void MQTTManager::playAnimation(const String &animationType)
{
  if (showingNotification)
  {
    Serial.println("Cannot play animation - notification in progress");
    return;
  }

  Serial.println("Playing animation: " + animationType);
  showingNotification = true; // Block other displays during animation

  if (animationType == "heart")
  {
    // Centered heart animation - 4 beats, longer duration
    Max72xxPanel &matrix = display.getMatrix();
    for (int repeat = 0; repeat < 4; repeat++)
    {
      // Frame 1: Small heart (centered on 32x8 display)
      display.fillScreen(LOW);
      matrix.drawPixel(14, 2, HIGH); // Centered at x=15-16
      matrix.drawPixel(15, 2, HIGH);
      matrix.drawPixel(17, 2, HIGH);
      matrix.drawPixel(18, 2, HIGH);
      matrix.drawPixel(13, 3, HIGH);
      matrix.drawPixel(16, 3, HIGH);
      matrix.drawPixel(19, 3, HIGH);
      matrix.drawPixel(14, 4, HIGH);
      matrix.drawPixel(18, 4, HIGH);
      matrix.drawPixel(15, 5, HIGH);
      matrix.drawPixel(17, 5, HIGH);
      matrix.drawPixel(16, 6, HIGH);
      display.write();
      delay(500); // Longer delay

      // Frame 2: Large heart
      display.fillScreen(LOW);
      matrix.drawPixel(13, 1, HIGH);
      matrix.drawPixel(14, 1, HIGH);
      matrix.drawPixel(15, 1, HIGH);
      matrix.drawPixel(17, 1, HIGH);
      matrix.drawPixel(18, 1, HIGH);
      matrix.drawPixel(19, 1, HIGH);
      matrix.drawPixel(12, 2, HIGH);
      matrix.drawPixel(16, 2, HIGH);
      matrix.drawPixel(20, 2, HIGH);
      matrix.drawPixel(12, 3, HIGH);
      matrix.drawPixel(20, 3, HIGH);
      matrix.drawPixel(13, 4, HIGH);
      matrix.drawPixel(19, 4, HIGH);
      matrix.drawPixel(14, 5, HIGH);
      matrix.drawPixel(18, 5, HIGH);
      matrix.drawPixel(15, 6, HIGH);
      matrix.drawPixel(17, 6, HIGH);
      matrix.drawPixel(16, 7, HIGH);
      display.write();
      delay(500); // Longer delay

      // Frame 3: Back to small
      display.fillScreen(LOW);
      matrix.drawPixel(14, 2, HIGH);
      matrix.drawPixel(15, 2, HIGH);
      matrix.drawPixel(17, 2, HIGH);
      matrix.drawPixel(18, 2, HIGH);
      matrix.drawPixel(13, 3, HIGH);
      matrix.drawPixel(16, 3, HIGH);
      matrix.drawPixel(19, 3, HIGH);
      matrix.drawPixel(14, 4, HIGH);
      matrix.drawPixel(18, 4, HIGH);
      matrix.drawPixel(15, 5, HIGH);
      matrix.drawPixel(17, 5, HIGH);
      matrix.drawPixel(16, 6, HIGH);
      display.write();
      delay(500); // Longer delay
    }
  }
  else if (animationType == "wave")
  {
    // Longer wave animation
    Max72xxPanel &matrix = display.getMatrix();
    for (int x = 0; x < 48; x++) // More frames for longer animation
    {
      display.fillScreen(LOW);
      for (int i = 0; i < 32; i += 2) // More points across display
      {
        int y = 4 + sin((x + i) * 0.3) * 2.5; // Smoother wave
        if (y >= 0 && y < 8)
        {
          matrix.drawPixel(i, y, HIGH);
        }
      }
      display.write();
      delay(80); // Slightly slower
    }
  }
  else if (animationType == "pulse")
  {
    // Simple expanding pulse animation (replaces star)
    Max72xxPanel &matrix = display.getMatrix();

    // Expand from center
    for (int radius = 1; radius <= 8; radius++)
    {
      display.fillScreen(LOW);

      // Draw expanding square/diamond
      for (int x = 16 - radius; x <= 16 + radius && x < 32; x++)
      {
        for (int y = 4 - radius / 2; y <= 4 + radius / 2 && y >= 0 && y < 8; y++)
        {
          if (x >= 0 && x < 32 && y >= 0 && y < 8)
          {
            if (abs(x - 16) + abs(y - 4) == radius || abs(x - 16) + abs(y - 4) == radius - 1)
            {
              matrix.drawPixel(x, y, HIGH);
            }
          }
        }
      }
      display.write();
      delay(200);
    }

    // Contract back to center
    for (int radius = 8; radius >= 1; radius--)
    {
      display.fillScreen(LOW);

      if (radius <= 3)
      {
        // Final small pulse in center
        matrix.drawPixel(16, 4, HIGH);
        if (radius > 1)
        {
          matrix.drawPixel(15, 4, HIGH);
          matrix.drawPixel(17, 4, HIGH);
          matrix.drawPixel(16, 3, HIGH);
          matrix.drawPixel(16, 5, HIGH);
        }
      }
      display.write();
      delay(200);
    }
  }
  else
  {
    // Unknown animation - show error pattern
    Serial.println("Unknown animation type: " + animationType + ". Available: heart, wave, pulse");
    for (int i = 0; i < 3; i++)
    {
      display.fillScreen(HIGH);
      delay(200);
      display.fillScreen(LOW);
      delay(200);
    }
  }

  showingNotification = false;

  // Update brightness in case day/night changed during animation
  updateBrightnessBasedOnTime();

  Serial.println("Animation completed - returning to clock display");
}
