#include "Arduino.h"
#include "Settings.h"
#include "DisplayManager.h"
#include "WiFiSetup.h"
#include "TimeManager.h"
#include "MQTTManager.h"
#include "OTAManager.h"
#include "WebOTAManager.h"
#include "BackgroundService.h"
#include <Ticker.h>

// Timing constants
const int LOOP_DELAY_MS = 100;                   // Main loop delay to prevent excessive CPU usage
const unsigned long WIFI_CHECK_INTERVAL = 30000; // Check WiFi every 30 seconds

// Global variables
int refresh = 0; // Used by DisplayManager to signal scroll refresh
Max72xxPanel matrix = Max72xxPanel(PIN_CS, NUMBER_OF_HORIZONTAL_DISPLAYS, NUMBER_OF_VERTICAL_DISPLAYS);
unsigned long lastWiFiCheck = 0;

// Set once setup() has initialized OTA/web/MQTT. Until then serviceBackground()
// only feeds the watchdog, so boot-time scrolls never touch an unstarted service.
bool servicesReady = false;

// Core components
TimeDB timeDB(TIMEZONE_DB_API_KEY);
DisplayManager displayManager(matrix);
WiFiSetup wifiSetup(displayManager);
TimeManager timeManager(timeDB, displayManager);
MQTTManager mqttManager(displayManager, timeManager);
OTAManager otaManager(displayManager);
WebOTAManager webOtaManager(displayManager);

// Keep background services alive during otherwise-blocking display operations
// (see BackgroundService.h). Feeding the watchdog is always safe; the network
// services are only pumped once setup() has started them.
void serviceBackground()
{
  ESP.wdtFeed();
  if (!servicesReady)
  {
    return;
  }
  otaManager.loop();
  webOtaManager.loop();
  mqttManager.keepAlive();
}

void serviceDelay(unsigned long ms)
{
  unsigned long start = millis();
  while (true)
  {
    serviceBackground();
    unsigned long elapsed = millis() - start;
    if (elapsed >= ms)
    {
      break;
    }
    unsigned long remaining = ms - elapsed;
    delay(remaining > 15 ? 15 : remaining);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(10);

  // Enable hardware watchdog (8 seconds timeout)
  ESP.wdtEnable(WDTO_8S);
  Serial.println("Watchdog enabled");

  // Initialize matrix display
  displayManager.initializeMatrix();

  // Set hostname
  wifiSetup.setHostname(DEVICE_HOSTNAME);

  // Brightness animation
  displayManager.performBrightnessAnimation();
  displayManager.setIntensity(DISPLAY_INTENSITY);

  // WiFi setup
  wifiSetup.initialize();

  // Initialize OTA and Web OTA BEFORE any other network service.
  // This guarantees firmware recovery is always reachable, even if a
  // later service (MQTT/time) is slow or unreachable.
  otaManager.initialize();
  webOtaManager.initialize();

  // Initialize MQTT last. Connection is established lazily in loop()
  // (see mqttManager.loop()), so this call never blocks startup.
  mqttManager.initialize();

  // All network services are up: allow serviceBackground() to pump them during
  // long display operations from now on.
  servicesReady = true;
}

void loop()
{
  // Feed the watchdog to prevent reset
  ESP.wdtFeed();

  // Check WiFi connection periodically
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL)
  {
    lastWiFiCheck = millis();
    wifiSetup.checkConnection();
  }

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

  delay(LOOP_DELAY_MS);
}
