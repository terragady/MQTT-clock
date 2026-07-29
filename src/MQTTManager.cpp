#include "MQTTManager.h"
#include "Settings.h"
#include <TimeLib.h>

// Static member initialization
MQTTManager *MQTTManager::instance = nullptr;

MQTTManager::MQTTManager(DisplayManager &displayRef, TimeManager &timeRef)
    : display(displayRef), timeManager(timeRef), mqttClient(wifiClient),
      dayBrightness(DEFAULT_DAY_BRIGHTNESS), nightBrightness(DEFAULT_NIGHT_BRIGHTNESS),
      dayStartMinutes(DEFAULT_DAY_START_MINUTES), nightStartMinutes(DEFAULT_NIGHT_START_MINUTES),
      showingNotification(false), lastReconnectAttempt(0), reconnectAttempts(0),
      lastStatusPublish(0), filesystemAvailable(false)
{
  instance = this; // Set static reference for callback
}

void MQTTManager::initialize()
{
  // Initialize LittleFS with retry
  for (int attempt = 0; attempt < 3; attempt++)
  {
    if (LittleFS.begin())
    {
      filesystemAvailable = true;
      Serial.println("LittleFS initialized successfully");
      loadSettings();
      break;
    }
    Serial.print("LittleFS initialization attempt ");
    Serial.print(attempt + 1);
    Serial.println(" failed, retrying...");
    delay(100);
  }

  if (!filesystemAvailable)
  {
    Serial.println("LittleFS initialization failed after 3 attempts! Using defaults.");
    Serial.println("Settings will not be persisted.");
  }

  // Cap how long the underlying TCP connect can block so a missing or
  // unreachable broker (e.g. an unresolved "homeassistant" hostname)
  // cannot stall the main loop.
  wifiClient.setTimeout(MQTT_CONNECT_TIMEOUT_MS);

  mqttClient.setServer(MQTT_SERVER.c_str(), MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S); // cap MQTT handshake/read wait

  Serial.println("MQTT Manager initialized");

  // Do NOT connect here. Connecting is handled lazily and rate-limited in
  // loop() -> tryReconnect(), so setup() never blocks on an unreachable
  // broker and OTA/Web/clock always come up.
}

void MQTTManager::loop()
{
  if (!mqttClient.connected())
  {
    tryReconnect();
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

  // Periodically refresh status so HA sensors (Day/Night mode, etc.) stay
  // current as the clock crosses a schedule boundary, even with no commands.
  if (mqttClient.connected() &&
      (millis() - lastStatusPublish >= MQTT_STATUS_PUBLISH_INTERVAL))
  {
    sendStatus("online");
  }
}

bool MQTTManager::tryReconnect()
{
  // Already connected
  if (mqttClient.connected())
  {
    reconnectAttempts = 0;
    return true;
  }

  // Check if enough time has passed since last attempt
  unsigned long now = millis();
  if (now - lastReconnectAttempt < MQTT_RECONNECT_INTERVAL)
  {
    return false;
  }

  lastReconnectAttempt = now;
  reconnectAttempts++;

  Serial.print("Attempting MQTT connection (attempt ");
  Serial.print(reconnectAttempts);
  Serial.print(")...");

  String clientId = MQTT_CLIENT_ID + "-" + String(random(0xffff), HEX);

  // When no username is configured, connect anonymously: we must NOT send
  // credentials at all. Sending an empty/incorrect username makes a broker
  // that allows anonymous access reject us with "not authorised".
  const String willMessage = "{\"status\":\"offline\"}";
  bool connected;
  if (MQTT_USER.length() == 0)
  {
    connected = mqttClient.connect(clientId.c_str(),
                                   MQTT_TOPIC_STATUS.c_str(), 0, true, willMessage.c_str());
  }
  else
  {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USER.c_str(), MQTT_PASSWORD.c_str(),
                                   MQTT_TOPIC_STATUS.c_str(), 0, true, willMessage.c_str());
  }

  if (connected)
  {
    Serial.println(" connected!");
    reconnectAttempts = 0;

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
    return true;
  }

  Serial.print(" failed, rc=");
  Serial.print(mqttClient.state());
  Serial.println(" will retry later");
  return false;
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
    payload += "\"day_start\":\"" + minutesToTimeString(dayStartMinutes) + "\",";
    payload += "\"night_start\":\"" + minutesToTimeString(nightStartMinutes) + "\",";
    payload += "\"is_day_time\":" + String(isDayTime() ? "true" : "false");
    payload += "}";

    mqttClient.publish(MQTT_TOPIC_STATUS.c_str(), payload.c_str(), true); // Retained status
    lastStatusPublish = millis();
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

  // 5. Notification Text Input. The json_attributes_topic surfaces the usage
  // docs (schema + examples) as entity attributes inside Home Assistant.
  String notification_config = "{"
                               "\"name\":\"Send Notification\","
                               "\"unique_id\":\"" +
                               device_id + "_notification\","
                                           "\"command_topic\":\"" +
                               MQTT_TOPIC_NOTIFICATION + "\","
                                                         "\"json_attributes_topic\":\"" +
                               MQTT_TOPIC_NOTIFICATION_HELP + "\","
                                                              "\"icon\":\"mdi:message-text\"," +
                               device_info + "}";

  mqttClient.publish("homeassistant/text/mqtt_clock/notification/config", notification_config.c_str(), true);

  // Publish the usage docs (retained) so they appear under the notification
  // entity's Attributes in Home Assistant.
  publishNotificationHelp();

  // 6. Animation Select (dropdown: heart / wave / pulse)
  String animation_config = "{"
                            "\"name\":\"Animation\","
                            "\"unique_id\":\"" +
                            device_id + "_animation\","
                                        "\"command_topic\":\"" +
                            MQTT_TOPIC_ANIMATION + "\","
                                                   "\"options\":[\"heart\",\"wave\",\"pulse\"],"
                                                   "\"icon\":\"mdi:animation-play\"," +
                            device_info + "}";

  mqttClient.publish("homeassistant/select/mqtt_clock/animation/config", animation_config.c_str(), true);

  // Remove the previous hour-only "number" entities (superseded by the time
  // entities below). Publishing an empty retained payload deletes a stale
  // discovery config from Home Assistant.
  mqttClient.publish("homeassistant/number/mqtt_clock/day_start/config", "", true);
  mqttClient.publish("homeassistant/number/mqtt_clock/night_start/config", "", true);

  // 7. Day Start Time (HH:MM). A Sun-based HA automation can write to this.
  String day_start_config = "{"
                            "\"name\":\"Day Start Time\","
                            "\"unique_id\":\"" +
                            device_id + "_day_start_time\","
                                        "\"state_topic\":\"" +
                            MQTT_TOPIC_STATUS + "\","
                                                "\"command_topic\":\"" +
                            MQTT_TOPIC_SCHEDULE_DAY_START + "\","
                                                           "\"value_template\":\"{{ value_json.day_start }}\","
                                                           "\"icon\":\"mdi:weather-sunset-up\"," +
                            device_info + "}";

  mqttClient.publish("homeassistant/time/mqtt_clock/day_start/config", day_start_config.c_str(), true);

  // 8. Night Start Time (HH:MM). A Sun-based HA automation can write to this.
  String night_start_config = "{"
                              "\"name\":\"Night Start Time\","
                              "\"unique_id\":\"" +
                              device_id + "_night_start_time\","
                                          "\"state_topic\":\"" +
                              MQTT_TOPIC_STATUS + "\","
                                                  "\"command_topic\":\"" +
                              MQTT_TOPIC_SCHEDULE_NIGHT_START + "\","
                                                               "\"value_template\":\"{{ value_json.night_start }}\","
                                                               "\"icon\":\"mdi:weather-sunset-down\"," +
                              device_info + "}";

  mqttClient.publish("homeassistant/time/mqtt_clock/night_start/config", night_start_config.c_str(), true);
}

void MQTTManager::publishNotificationHelp()
{
  if (!mqttClient.connected())
  {
    return;
  }

  // Each key becomes an attribute on the "Send Notification" entity in HA.
  // Keep this compact so the whole packet stays within MQTT_BUFFER_SIZE.
  String help = "{"
                "\"usage\":\"Publish plain text (scrolls once) or a JSON object\","
                "\"message\":\"string, required\","
                "\"scrolling\":\"bool, default true; false = static/centered\","
                "\"speed\":\"int 5-100 ms, default 35; lower = faster\","
                "\"repeat\":\"int 1-10, default 1\","
                "\"brightness\":\"int 0-15 or -1, default -1 (keep current)\","
                "\"flash\":\"bool, default false (static only)\","
                "\"flash_count\":\"int 1-10, default 3\","
                "\"example\":\"{\\\"message\\\":\\\"Dinner!\\\",\\\"speed\\\":15,\\\"repeat\\\":2}\","
                "\"animation\":\"publish heart|wave|pulse to " +
                MQTT_TOPIC_ANIMATION + "\""
                                       "}";

  mqttClient.publish(MQTT_TOPIC_NOTIFICATION_HELP.c_str(), help.c_str(), true);
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
    int minutes = parseTimeStringToMinutes(message);
    if (minutes >= 0)
    {
      setDayStartMinutes(minutes);
    }
  }
  else if (topic == MQTT_TOPIC_SCHEDULE_NIGHT_START)
  {
    int minutes = parseTimeStringToMinutes(message);
    if (minutes >= 0)
    {
      setNightStartMinutes(minutes);
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

  display.scrollMessage(message);

  showingNotification = false;
  updateBrightnessBasedOnTime();
}

void MQTTManager::setDayBrightness(int brightness)
{
  dayBrightness = constrain(brightness, 0, 15);
  updateBrightnessBasedOnTime();
  saveSettings();
}

void MQTTManager::setNightBrightness(int brightness)
{
  nightBrightness = constrain(brightness, 0, 15);
  updateBrightnessBasedOnTime();
  saveSettings();
}

void MQTTManager::setDayStartMinutes(int minutes)
{
  dayStartMinutes = constrain(minutes, 0, 1439);
  updateBrightnessBasedOnTime();
  saveSettings();
}

void MQTTManager::setNightStartMinutes(int minutes)
{
  nightStartMinutes = constrain(minutes, 0, 1439);
  updateBrightnessBasedOnTime();
  saveSettings();
}

String MQTTManager::minutesToTimeString(int minutes)
{
  minutes = constrain(minutes, 0, 1439);
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:00", minutes / 60, minutes % 60);
  return String(buffer);
}

int MQTTManager::parseTimeStringToMinutes(const String &value)
{
  int firstColon = value.indexOf(':');
  if (firstColon < 0)
  {
    return -1;
  }

  int parsedHour = value.substring(0, firstColon).toInt();

  int secondColon = value.indexOf(':', firstColon + 1);
  int parsedMinute = (secondColon < 0)
                         ? value.substring(firstColon + 1).toInt()
                         : value.substring(firstColon + 1, secondColon).toInt();

  if (parsedHour < 0 || parsedHour > 23 || parsedMinute < 0 || parsedMinute > 59)
  {
    return -1;
  }

  return parsedHour * 60 + parsedMinute;
}

void MQTTManager::updateBrightnessBasedOnTime()
{
  int targetBrightness = isDayTime() ? dayBrightness : nightBrightness;
  display.setIntensity(targetBrightness);
}

bool MQTTManager::isDayTime()
{
  int currentMinutes = hour() * 60 + minute();

  // Handle normal case (day start < night start)
  if (dayStartMinutes < nightStartMinutes)
  {
    return (currentMinutes >= dayStartMinutes && currentMinutes < nightStartMinutes);
  }
  // Handle wrap-around case (night start < day start, e.g. day 06:30, night 22:00)
  else
  {
    return (currentMinutes >= dayStartMinutes || currentMinutes < nightStartMinutes);
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

  // Prefer the new minute-based keys. Fall back to the legacy hour-only keys
  // (from firmware before HH:MM support) so existing devices keep their schedule.
  if (doc["day_start_minutes"].is<int>())
  {
    dayStartMinutes = doc["day_start_minutes"];
  }
  else
  {
    dayStartMinutes = (doc["day_start_hour"] | (DEFAULT_DAY_START_MINUTES / 60)) * 60;
  }

  if (doc["night_start_minutes"].is<int>())
  {
    nightStartMinutes = doc["night_start_minutes"];
  }
  else
  {
    nightStartMinutes = (doc["night_start_hour"] | (DEFAULT_NIGHT_START_MINUTES / 60)) * 60;
  }

  dayStartMinutes = constrain(dayStartMinutes, 0, 1439);
  nightStartMinutes = constrain(nightStartMinutes, 0, 1439);
}

void MQTTManager::saveSettings()
{
  if (!filesystemAvailable)
  {
    Serial.println("Filesystem not available, cannot save settings");
    return;
  }

  JsonDocument doc;

  doc["day_brightness"] = dayBrightness;
  doc["night_brightness"] = nightBrightness;
  doc["day_start_minutes"] = dayStartMinutes;
  doc["night_start_minutes"] = nightStartMinutes;

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
  else
  {
    Serial.println("Settings saved successfully");
  }

  file.close();
}

void MQTTManager::showAdvancedNotification(const NotificationConfig &config)
{
  currentConfig = config;
  currentNotification = config.message;
  showingNotification = true;

  // Temporarily set brightness if specified
  if (config.brightness >= 0)
  {
    display.setIntensity(config.brightness);
  }

  Serial.println("Showing advanced notification: " + config.message);

  if (config.isScrolling)
  {
    // Perform scrolling repeats
    for (int i = 0; i < config.scrollRepeat; i++)
    {
      display.scrollMessage(config.message, config.scrollSpeed);
      if (i < config.scrollRepeat - 1)
        delay(500); // Brief pause between repeats
    }

    showingNotification = false;
    updateBrightnessBasedOnTime();
  }
  else
  {
    // Static notification
    display.fillScreen(LOW);
    display.centerPrint(config.message);

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
      // No flash effect - show message for 3 seconds
      delay(3000);
    }

    // Static notification is done, return to clock
    showingNotification = false;
    updateBrightnessBasedOnTime();
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
    return; // Skip if notification in progress
  }

  showingNotification = true; // Block other displays during animation

  if (animationType == "heart")
  {
    // Heart animation - 4 beats
    Max72xxPanel &matrix = display.getMatrix();
    for (int repeat = 0; repeat < 4; repeat++)
    {
      // Small heart
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
      delay(500);

      // Large heart
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
      delay(500);
    }
  }
  else if (animationType == "wave")
  {
    // Wave animation
    Max72xxPanel &matrix = display.getMatrix();
    for (int x = 0; x < 48; x++)
    {
      display.fillScreen(LOW);
      for (int i = 0; i < 32; i += 2)
      {
        int y = 4 + sin((x + i) * 0.3) * 2.5;
        if (y >= 0 && y < 8)
        {
          matrix.drawPixel(i, y, HIGH);
        }
      }
      display.write();
      delay(80);
    }
  }
  else if (animationType == "pulse")
  {
    // Pulse animation
    Max72xxPanel &matrix = display.getMatrix();

    // Expand from center
    for (int radius = 1; radius <= 8; radius++)
    {
      display.fillScreen(LOW);
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
    for (int i = 0; i < 3; i++)
    {
      display.fillScreen(HIGH);
      delay(200);
      display.fillScreen(LOW);
      delay(200);
    }
  }

  showingNotification = false;
  updateBrightnessBasedOnTime();
}
