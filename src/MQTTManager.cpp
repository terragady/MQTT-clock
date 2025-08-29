#include "MQTTManager.h"
#include "Settings.h"
#include <TimeLib.h>

// Static member initialization
MQTTManager *MQTTManager::instance = nullptr;

MQTTManager::MQTTManager(DisplayManager &displayRef, TimeManager &timeRef)
    : display(displayRef), timeManager(timeRef), mqttClient(wifiClient),
      dayBrightness(DEFAULT_DAY_BRIGHTNESS), nightBrightness(DEFAULT_NIGHT_BRIGHTNESS),
      dayStartHour(DEFAULT_DAY_START_HOUR), nightStartHour(DEFAULT_NIGHT_START_HOUR),
      showingNotification(false), notificationStartTime(0)
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

  Serial.println("MQTT Manager initialized");
  Serial.println("Server: " + MQTT_SERVER + ":" + String(MQTT_PORT));

  reconnect();
}

void MQTTManager::loop()
{
  if (!mqttClient.connected())
  {
    reconnect();
  }
  mqttClient.loop();

  // Update brightness based on current time
  updateBrightnessBasedOnTime();
}

void MQTTManager::reconnect()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");

    String clientId = MQTT_CLIENT_ID + "-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USER.c_str(), MQTT_PASSWORD.c_str()))
    {
      Serial.println(" connected!");

      // Subscribe to topics
      mqttClient.subscribe(MQTT_TOPIC_NOTIFICATION.c_str());
      mqttClient.subscribe(MQTT_TOPIC_BRIGHTNESS_DAY.c_str());
      mqttClient.subscribe(MQTT_TOPIC_BRIGHTNESS_NIGHT.c_str());
      mqttClient.subscribe(MQTT_TOPIC_SCHEDULE_DAY_START.c_str());
      mqttClient.subscribe(MQTT_TOPIC_SCHEDULE_NIGHT_START.c_str());

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

    mqttClient.publish(MQTT_TOPIC_STATUS.c_str(), payload.c_str());
  }
}

void MQTTManager::sendDiscoveryConfig()
{
  if (mqttClient.connected())
  {
    String config = "{";
    config += "\"name\":\"MQTT Clock\",";
    config += "\"unique_id\":\"mqtt_clock_zegarTV\",";
    config += "\"command_topic\":\"" + MQTT_TOPIC_NOTIFICATION + "\",";
    config += "\"state_topic\":\"" + MQTT_TOPIC_STATUS + "\",";
    config += "\"device\":{";
    config += "\"identifiers\":[\"mqtt_clock\"],";
    config += "\"name\":\"MQTT Clock\",";
    config += "\"model\":\"ESP8266 LED Matrix\",";
    config += "\"manufacturer\":\"Custom\"";
    config += "}";
    config += "}";

    mqttClient.publish(MQTT_TOPIC_DISCOVERY.c_str(), config.c_str(), true);
    Serial.println("Sent Home Assistant discovery config");
  }
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
  Serial.println("MQTT message received:");
  Serial.println("Topic: " + topic);
  Serial.println("Message: " + message);

  if (topic == MQTT_TOPIC_NOTIFICATION)
  {
    showNotification(message);
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

  // Send updated status
  sendStatus("online");
}

void MQTTManager::showNotification(const String &message)
{
  currentNotification = message;
  showingNotification = true;
  notificationStartTime = millis();

  Serial.println("Showing notification: " + message);
  display.scrollMessage(message);

  // Reset notification flag after scroll completes
  showingNotification = false;
  Serial.println("Notification display completed - returning to clock display");
}

void MQTTManager::setDayBrightness(int brightness)
{
  dayBrightness = constrain(brightness, 0, 15);
  Serial.println("Day brightness set to: " + String(dayBrightness));
  updateBrightnessBasedOnTime();
  saveSettings(); // Save to SPIFFS
}

void MQTTManager::setNightBrightness(int brightness)
{
  nightBrightness = constrain(brightness, 0, 15);
  Serial.println("Night brightness set to: " + String(nightBrightness));
  updateBrightnessBasedOnTime();
  saveSettings(); // Save to SPIFFS
}

void MQTTManager::setDayStartHour(int hour)
{
  dayStartHour = constrain(hour, 0, 23);
  Serial.println("Day start hour set to: " + String(dayStartHour));
  updateBrightnessBasedOnTime();
  saveSettings(); // Save to SPIFFS
}

void MQTTManager::setNightStartHour(int hour)
{
  nightStartHour = constrain(hour, 0, 23);
  Serial.println("Night start hour set to: " + String(nightStartHour));
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

  Serial.println("Settings loaded from SPIFFS:");
  Serial.println("Day brightness: " + String(dayBrightness));
  Serial.println("Night brightness: " + String(nightBrightness));
  Serial.println("Day start hour: " + String(dayStartHour));
  Serial.println("Night start hour: " + String(nightStartHour));
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
  else
  {
    Serial.println("Settings saved to SPIFFS");
  }

  file.close();
}
