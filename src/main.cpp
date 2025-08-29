#include "Arduino.h"
#include "Settings.h"
#include "DisplayManager.h"
#include "WiFiSetup.h"
#include "TimeManager.h"

// Global variables
int refresh = 0;
Max72xxPanel matrix = Max72xxPanel(PIN_CS, NUMBER_OF_HORIZONTAL_DISPLAYS, NUMBER_OF_VERTICAL_DISPLAYS);

// Core components
TimeDB timeDB(TIMEZONE_DB_API_KEY);
DisplayManager displayManager(matrix);
WiFiSetup wifiSetup(displayManager);
TimeManager timeManager(timeDB, displayManager);

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
}

void loop()
{
  // Update time if needed
  if (timeManager.shouldUpdateTime())
  {
    timeManager.updateTime();
  }

  // Update display when minute changes
  timeManager.hasMinuteChanged();

  // Display current time
  String currentTime = timeManager.getFormattedTime(false);
  displayManager.fillScreen(LOW);
  displayManager.centerPrint(currentTime);

  delay(100); // Small delay to prevent excessive CPU usage
}
