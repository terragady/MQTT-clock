#include "MQTTManager.h"
#include "Settings.h"
#include "BackgroundService.h"
#include "TimeOfDay.h"

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

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
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

void MQTTManager::keepAlive()
{
  // Pump only the MQTT client (keepalive + incoming). Intentionally does NOT
  // run the notification queue or brightness logic, so it is safe to call from
  // inside a blocking display operation without re-entering it.
  if (mqttClient.connected())
  {
    mqttClient.loop();
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

  // A stable, MAC-derived client id. Generating a random id per attempt made
  // the broker treat every reconnect as a brand-new session, so the *previous*
  // session's Last Will could fire and publish "offline" after the new session
  // had already published "online" - leaving Home Assistant showing the clock
  // as unavailable while it was actually running fine.
  const String clientId = getDeviceId();

  // When no username is configured, connect anonymously: we must NOT send
  // credentials at all. Sending an empty/incorrect username makes a broker
  // that allows anonymous access reject us with "not authorised".
  const String willMessage = "{\"status\":\"offline\"}";
  bool connected;
  if (MQTT_USER[0] == '\0')
  {
    connected = mqttClient.connect(clientId.c_str(),
                                   MQTT_TOPIC_STATUS, 0, true, willMessage.c_str());
  }
  else
  {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                                   MQTT_TOPIC_STATUS, 0, true, willMessage.c_str());
  }

  if (connected)
  {
    Serial.println(" connected!");
    reconnectAttempts = 0;

    // Subscribe to topics
    mqttClient.subscribe(MQTT_TOPIC_NOTIFICATION);
    mqttClient.subscribe(MQTT_TOPIC_ANIMATION);
    mqttClient.subscribe(MQTT_TOPIC_BRIGHTNESS_DAY);
    mqttClient.subscribe(MQTT_TOPIC_BRIGHTNESS_NIGHT);
    mqttClient.subscribe(MQTT_TOPIC_SCHEDULE_DAY_START);
    mqttClient.subscribe(MQTT_TOPIC_SCHEDULE_NIGHT_START);
    mqttClient.subscribe(MQTT_TOPIC_DISCOVERY);

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
    // Report the mode the display is actually in. Before the first time sync
    // the clock reads the 1970 epoch, which would say "Night" while
    // currentAutoBrightness() is deliberately running day brightness.
    const bool dayTime = timeManager.isTimeSynced() ? isDayTime() : true;
    payload += "\"is_day_time\":" + String(dayTime ? "true" : "false") + ",";
    payload += "\"time_synced\":" + String(timeManager.isTimeSynced() ? "true" : "false");
    payload += "}";

    mqttClient.publish(MQTT_TOPIC_STATUS, payload.c_str(), true); // Retained status
    lastStatusPublish = millis();
  }
}

String MQTTManager::getDeviceId()
{
  String deviceId = "mqtt_clock_" + WiFi.macAddress();
  deviceId.replace(":", "");
  return deviceId;
}

String MQTTManager::buildDeviceInfo()
{
  return "\"device\":{"
         "\"identifiers\":[\"" +
         getDeviceId() + "\"],"
                         "\"name\":\"MQTT Clock\","
                         "\"model\":\"ESP8266 LED Matrix Clock\","
                         "\"manufacturer\":\"Custom\","
                         "\"sw_version\":\"1.0\""
                         "}";
}

void MQTTManager::publishDiscovery(const String &component, const String &objectId, const String &fields)
{
  if (!mqttClient.connected())
  {
    return;
  }
  String payload = "{" + fields + "," + buildDeviceInfo() + "}";
  String topic = "homeassistant/" + component + "/mqtt_clock/" + objectId + "/config";
  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void MQTTManager::sendDiscoveryConfig()
{
  if (!mqttClient.connected())
  {
    return;
  }

  const String id = getDeviceId();

  // 1. Status Sensor
  publishDiscovery("sensor", "status",
                   "\"name\":\"Clock Status\","
                   "\"unique_id\":\"" +
                       id + "_status\","
                            "\"state_topic\":\"" +
                       MQTT_TOPIC_STATUS + "\","
                                           "\"value_template\":\"{{ value_json.status }}\","
                                           "\"icon\":\"mdi:clock-digital\"");

  // 2. Day/Night Mode Sensor
  publishDiscovery("sensor", "daynight",
                   "\"name\":\"Day/Night Mode\","
                   "\"unique_id\":\"" +
                       id + "_daynight\","
                            "\"state_topic\":\"" +
                       MQTT_TOPIC_STATUS + "\","
                                           "\"value_template\":\"{% if value_json.is_day_time %}Day{% else %}Night{% endif %}\","
                                           "\"icon\":\"mdi:weather-sunny\"");

  // 3. Day Brightness Number Control
  publishDiscovery("number", "day_brightness",
                   "\"name\":\"Day Brightness\","
                   "\"unique_id\":\"" +
                       id + "_day_brightness\","
                            "\"state_topic\":\"" +
                       MQTT_TOPIC_STATUS + "\","
                                           "\"command_topic\":\"" +
                       MQTT_TOPIC_BRIGHTNESS_DAY + "\","
                                                   "\"value_template\":\"{{ value_json.day_brightness }}\","
                                                   "\"min\":0,\"max\":15,\"step\":1,"
                                                   "\"icon\":\"mdi:brightness-6\"");

  // 4. Night Brightness Number Control
  publishDiscovery("number", "night_brightness",
                   "\"name\":\"Night Brightness\","
                   "\"unique_id\":\"" +
                       id + "_night_brightness\","
                            "\"state_topic\":\"" +
                       MQTT_TOPIC_STATUS + "\","
                                           "\"command_topic\":\"" +
                       MQTT_TOPIC_BRIGHTNESS_NIGHT + "\","
                                                     "\"value_template\":\"{{ value_json.night_brightness }}\","
                                                     "\"min\":0,\"max\":15,\"step\":1,"
                                                     "\"icon\":\"mdi:brightness-3\"");

  // 5. Notification Text Input. The json_attributes_topic surfaces the usage
  // docs (schema + examples) as entity attributes inside Home Assistant.
  publishDiscovery("text", "notification",
                   "\"name\":\"Send Notification\","
                   "\"unique_id\":\"" +
                       id + "_notification\","
                            "\"command_topic\":\"" +
                       MQTT_TOPIC_NOTIFICATION + "\","
                                                 "\"json_attributes_topic\":\"" +
                       MQTT_TOPIC_NOTIFICATION_HELP + "\","
                                                      "\"icon\":\"mdi:message-text\"");

  // Publish the usage docs (retained) so they appear under the notification
  // entity's Attributes in Home Assistant.
  publishNotificationHelp();

  // 6. Animation Select (dropdown: heart / wave / pulse)
  publishDiscovery("select", "animation",
                   "\"name\":\"Animation\","
                   "\"unique_id\":\"" +
                       id + "_animation\","
                            "\"command_topic\":\"" +
                       MQTT_TOPIC_ANIMATION + "\","
                                              "\"options\":[\"heart\",\"wave\",\"pulse\"],"
                                              "\"icon\":\"mdi:animation-play\"");

  // Remove the previous hour-only "number" entities (superseded by the time
  // entities below). Publishing an empty retained payload deletes a stale
  // discovery config from Home Assistant.
  mqttClient.publish("homeassistant/number/mqtt_clock/day_start/config", "", true);
  mqttClient.publish("homeassistant/number/mqtt_clock/night_start/config", "", true);

  // 7. Day Start Time (HH:MM). A Sun-based HA automation can write to this.
  publishDiscovery("time", "day_start",
                   "\"name\":\"Day Start Time\","
                   "\"unique_id\":\"" +
                       id + "_day_start_time\","
                            "\"state_topic\":\"" +
                       MQTT_TOPIC_STATUS + "\","
                                           "\"command_topic\":\"" +
                       MQTT_TOPIC_SCHEDULE_DAY_START + "\","
                                                      "\"value_template\":\"{{ value_json.day_start }}\","
                                                      "\"icon\":\"mdi:weather-sunset-up\"");

  // 8. Night Start Time (HH:MM). A Sun-based HA automation can write to this.
  publishDiscovery("time", "night_start",
                   "\"name\":\"Night Start Time\","
                   "\"unique_id\":\"" +
                       id + "_night_start_time\","
                            "\"state_topic\":\"" +
                       MQTT_TOPIC_STATUS + "\","
                                           "\"command_topic\":\"" +
                       MQTT_TOPIC_SCHEDULE_NIGHT_START + "\","
                                                        "\"value_template\":\"{{ value_json.night_start }}\","
                                                        "\"icon\":\"mdi:weather-sunset-down\"");
}

void MQTTManager::publishNotificationHelp()
{
  if (!mqttClient.connected())
  {
    return;
  }

  // Each key becomes an attribute on the "Send Notification" entity in HA.
  // Keep this compact so the whole packet stays within MQTT_BUFFER_SIZE.
  // One string literal, assembled by the compiler. The topic name is spliced in
  // via the prefix macro, so this costs no heap and no runtime concatenation.
  static const char help[] PROGMEM =
      "{"
      "\"usage\":\"Publish plain text (scrolls once) or a JSON object\","
      "\"message\":\"string, required\","
      "\"scrolling\":\"bool, default true; false = static/centered\","
      "\"speed\":\"int 5-100 ms, default 35; lower = faster\","
      "\"repeat\":\"int 1-10, default 1\","
      "\"brightness\":\"int 0-15 or -1, default -1 (keep current)\","
      "\"duration\":\"int 1-30 s, default 3; static hold time\","
      "\"flash\":\"bool, default false; quick fade out/in before holding (static only)\","
      "\"flash_count\":\"int 1-10, default 2; number of fade pulses\","
      "\"example\":\"{\\\"message\\\":\\\"Dinner!\\\",\\\"scrolling\\\":false,\\\"flash\\\":true}\","
      "\"animation\":\"publish heart|wave|pulse to " MQTT_TOPIC_PREFIX_LITERAL "/animation\""
      "}";

  // Copied out of flash only for the moment it takes to publish.
  const String helpPayload = FPSTR(help);
  mqttClient.publish(MQTT_TOPIC_NOTIFICATION_HELP, helpPayload.c_str(), true);
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
  else if (topic == MQTT_TOPIC_DISCOVERY)
  {
    sendDiscoveryConfig();
  }
  else if (topic == MQTT_TOPIC_ANIMATION)
  {
    // Queue rather than play inline: this runs inside the PubSubClient
    // callback, and an animation blocks for seconds while calling
    // serviceDelay() -> keepAlive() -> mqttClient.loop(), which would
    // re-enter PubSubClient from inside its own callback.
    NotificationConfig job;
    job.kind = QueuedJobKind::Animation;
    job.message = message;
    queueNotification(job);
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

// The four setters below all bail out when the value is unchanged. Home
// Assistant replays retained values on every reconnect, and the documented
// sun-based automation rewrites the schedule daily, so without this guard each
// of those would erase-and-rewrite the LittleFS sector for no reason. ESP8266
// flash is only good for ~10k-100k erase cycles per sector.

void MQTTManager::setDayBrightness(int brightness)
{
  const int clamped = constrain(brightness, 0, 15);
  if (clamped == dayBrightness)
  {
    return;
  }
  dayBrightness = clamped;
  updateBrightnessBasedOnTime();
  saveSettings();
}

void MQTTManager::setNightBrightness(int brightness)
{
  const int clamped = constrain(brightness, 0, 15);
  if (clamped == nightBrightness)
  {
    return;
  }
  nightBrightness = clamped;
  updateBrightnessBasedOnTime();
  saveSettings();
}

void MQTTManager::setDayStartMinutes(int minutes)
{
  const int clamped = constrain(minutes, 0, 1439);
  if (clamped == dayStartMinutes)
  {
    return;
  }
  dayStartMinutes = clamped;
  updateBrightnessBasedOnTime();
  saveSettings();
}

void MQTTManager::setNightStartMinutes(int minutes)
{
  const int clamped = constrain(minutes, 0, 1439);
  if (clamped == nightStartMinutes)
  {
    return;
  }
  nightStartMinutes = clamped;
  updateBrightnessBasedOnTime();
  saveSettings();
}

String MQTTManager::minutesToTimeString(int minutes)
{
  char buffer[9];
  formatMinutesAsTime(minutes, buffer, sizeof(buffer));
  return String(buffer);
}

int MQTTManager::parseTimeStringToMinutes(const String &value)
{
  return parseTimeToMinutes(value.c_str());
}

int MQTTManager::currentAutoBrightness()
{
  // Before the first successful time sync the clock reads the 1970 epoch
  // (around midnight), which would wrongly select night mode and blank the
  // display. Until we actually know the time, use the (brighter,
  // always-visible) day brightness.
  if (!timeManager.isTimeSynced())
  {
    return dayBrightness;
  }
  return isDayTime() ? dayBrightness : nightBrightness;
}

void MQTTManager::updateBrightnessBasedOnTime()
{
  display.setIntensity(currentAutoBrightness());
}

bool MQTTManager::isDayTime()
{
  return isWithinDayWindow(timeManager.minutesSinceMidnight(), dayStartMinutes, nightStartMinutes);
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

  // A static message that is wider than the panel would be clipped and
  // unreadable, so scroll it instead of honouring the static request.
  const bool mustScroll = config.isScrolling || !display.textFits(config.message);

  if (mustScroll)
  {
    // Perform scrolling repeats
    for (int i = 0; i < config.scrollRepeat; i++)
    {
      display.scrollMessage(config.message, config.scrollSpeed);
      if (i < config.scrollRepeat - 1)
        serviceDelay(500); // Brief pause between repeats
    }

    showingNotification = false;
    updateBrightnessBasedOnTime();
  }
  else
  {
    // Static notification. The brightness to settle at once done.
    int holdBrightness = (config.brightness >= 0) ? config.brightness : currentAutoBrightness();

    // Draw the message once; the fade only modulates panel intensity, so the
    // text stays on screen throughout.
    display.fillScreen(LOW);
    display.centerPrint(config.message);
    display.setIntensity(holdBrightness);

    // Optionally fade the message out and back in a few times to grab
    // attention, then hold it steady for the configured duration.
    if (config.flashEffect)
    {
      for (int i = 0; i < config.flashCount; i++)
      {
        display.fadeMessage(holdBrightness, NOTIFICATION_FADE_STEP_MS);
      }
    }

    // Hold the message steady at the set brightness.
    display.setIntensity(holdBrightness);
    serviceDelay((unsigned long)config.holdSeconds * 1000UL);

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

  const String message = doc["message"] | "";
  if (message.length() == 0)
  {
    Serial.println("Notification JSON has no message, ignoring");
    return;
  }

  // Read each field into a local first: constrain() is a macro that evaluates
  // its argument three times, which would mean three JSON lookups per field.
  const int repeat = doc["repeat"] | 1;
  const int speed = doc["speed"] | 35;
  const int brightness = doc["brightness"] | -1;
  const int flashCount = doc["flash_count"] | 2;
  const int duration = doc["duration"] | 3;

  NotificationConfig config;
  config.message = message;
  config.isScrolling = doc["scrolling"] | true;
  config.flashEffect = doc["flash"] | false;
  config.scrollRepeat = constrain(repeat, 1, 10);
  config.scrollSpeed = constrain(speed, 5, 100);
  config.brightness = constrain(brightness, -1, 15);
  config.flashCount = constrain(flashCount, 1, 10);
  config.holdSeconds = constrain(duration, 1, 30);
  config.isSimpleMessage = false;

  queueNotification(config);
}

void MQTTManager::queueNotification(const NotificationConfig &config)
{
  if (notificationQueue.size() >= MAX_NOTIFICATION_QUEUE_LENGTH)
  {
    Serial.println("Notification queue full, dropping: " + config.message);
    return;
  }

  NotificationConfig job = config;
  if (job.message.length() > MAX_NOTIFICATION_MESSAGE_CHARS)
  {
    job.message = job.message.substring(0, MAX_NOTIFICATION_MESSAGE_CHARS);
    Serial.println("Notification message truncated to " + String(MAX_NOTIFICATION_MESSAGE_CHARS) + " chars");
  }

  notificationQueue.push(job);
  Serial.println("Notification queued: " + job.message + " (Queue size: " + String(notificationQueue.size()) + ")");
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

  Serial.println("Processing job from queue: " + config.message + " (Remaining in queue: " + String(notificationQueue.size()) + ")");

  // Everything here blocks for a while, but we are called from loop(), not from
  // the MQTT callback, so blocking is safe.
  if (config.kind == QueuedJobKind::Animation)
  {
    playAnimation(config.message);
  }
  else if (config.isSimpleMessage)
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
      serviceDelay(500);

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
      serviceDelay(500);
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
      serviceDelay(80);
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
      serviceDelay(200);
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
      serviceDelay(200);
    }
  }
  else
  {
    // Unknown animation - show error pattern
    for (int i = 0; i < 3; i++)
    {
      display.fillScreen(HIGH);
      serviceDelay(200);
      display.fillScreen(LOW);
      serviceDelay(200);
    }
  }

  showingNotification = false;
  updateBrightnessBasedOnTime();
}
