#include "Arduino.h"
#include "Settings.h"
#include "DisplayManager.h"
#include "WiFiSetup.h"
#include "TimeManager.h"
#include "MQTTManager.h"
#include "OTAManager.h"
#include "WebOTAManager.h"

// Global variables
int refresh = 0;
Max72xxPanel matrix = Max72xxPanel(PIN_CS, NUMBER_OF_HORIZONTAL_DISPLAYS, NUMBER_OF_VERTICAL_DISPLAYS);

// Core components
TimeDB timeDB(TIMEZONE_DB_API_KEY);
DisplayManager displayManager(matrix);
WiFiSetup wifiSetup(displayManager);
TimeManager timeManager(timeDB, displayManager);
MQTTManager mqttManager(displayManager, timeManager);
OTAManager otaManager(displayManager);
WebOTAManager webOtaManager(displayManager);

void setup()
{
  Serial.begin(115200);
  delay(10);

  // Initialize matrix display
  displayManager.initializeMatrix();

  // Set hostname
  wifiSetup.setHostname(DEVICE_HOSTNAME);

  // Brightness animation
  displayManager.performBrightnessAnimation();
  displayManager.setIntensity(DISPLAY_INTENSITY);

  // WiFi setup
  wifiSetup.initialize();

  // Initialize MQTT
  mqttManager.initialize();

  // Initialize OTA
  otaManager.initialize();

  // Initialize Web OTA
  webOtaManager.initialize();
}

void loop()
{
  // Handle OTA updates
  otaManager.loop();

  // Handle Web OTA
  webOtaManager.loop();

  // Handle MQTT
  mqttManager.loop();

  // Update time if needed
  if (timeManager.shouldUpdateTime())
  {
    timeManager.updateTime();
  }

  // Only show clock if not displaying notification
  if (!mqttManager.isShowingNotification())
  {
    // Update display when minute changes
    timeManager.hasMinuteChanged();

    // Display current time
    String currentTime = timeManager.getFormattedTime(false);
    displayManager.fillScreen(LOW);
    displayManager.centerPrint(currentTime);
  }

  delay(100); // Small delay to prevent excessive CPU usage
}
