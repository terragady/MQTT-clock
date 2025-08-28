#include "Arduino.h"
#include "Settings.h"

// Constants
const int TIMEOUT_MS = 500; // 500ms timeout
const int FONT_WIDTH = 5;
const int SPACER = 1;
const int CHAR_WIDTH = FONT_WIDTH + SPACER;

// Global variables
int refresh = 0;
Max72xxPanel matrix = Max72xxPanel(PIN_CS, NUMBER_OF_HORIZONTAL_DISPLAYS, NUMBER_OF_VERTICAL_DISPLAYS);

// Time tracking
TimeDB timeDB(TIMEZONE_DB_API_KEY);
String lastMinute = "xx";
long lastEpoch = 0;
long firstEpoch = 0;
int timeoutCount = 0;

// Function declarations
void performBrightnessAnimation();
void setupWiFi();
void configModeCallback(WiFiManager *myWiFiManager);

String secondsIndicator(bool isRefresh)
{
  String rtnValue = ":";
  if (!isRefresh && FLASH_ON_SECONDS && (second() % 2) == 0)
  {
    rtnValue = " ";
  }
  return rtnValue;
}

String hourMinutes(bool isRefresh)
{
  return String(hour()) + secondsIndicator(isRefresh) + timeDB.zeroPad(minute());
}

void scrollMessage(String msg)
{
  msg += " "; // add a space at the end
  for (int i = 0; i < (int)(CHAR_WIDTH * msg.length() + matrix.width() - 1 - SPACER); i++)
  {
    if (refresh == 1)
    {
      i = 0;
    }
    refresh = 0;
    matrix.fillScreen(LOW);

    int letter = i / CHAR_WIDTH;
    int x = (matrix.width() - 1) - i % CHAR_WIDTH;
    int y = (matrix.height() - 8) / 2; // center the text vertically

    while (x + CHAR_WIDTH - SPACER >= 0 && letter >= 0)
    {
      if (letter < (int)msg.length())
      {
        matrix.drawChar(x, y, msg[letter], HIGH, LOW, 1);
      }
      letter--;
      x -= CHAR_WIDTH;
    }

    matrix.write(); // Send bitmap to display
    delay(DISPLAY_SCROLL_SPEED);
  }
  matrix.setCursor(0, 0);
}

void updateTime()
{
  Serial.println("Updating Time...");

  // Show update indicator
  matrix.drawPixel(0, 4, HIGH);
  matrix.drawPixel(0, 3, HIGH);
  matrix.drawPixel(0, 2, HIGH);
  matrix.write();

  time_t currentTime = timeDB.getTime();
  if (currentTime > 5000)
  {
    setTime(currentTime);
    Serial.println("Time updated successfully");
  }
  else
  {
    Serial.println("Time update failed!");
    scrollMessage("Time update failed!");
    return; // Don't update lastEpoch if time update failed
  }

  lastEpoch = now();
  if (firstEpoch == 0)
  {
    firstEpoch = now();
  }
}

void centerPrint(String msg)
{
  int x = (matrix.width() - (msg.length() * CHAR_WIDTH)) / 2;
  matrix.setCursor(x, 0);
  matrix.print(msg);
  matrix.write();
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println("Wifi Manager");
  Serial.println("Please connect to AP");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println("To setup Wifi Configuration");
  scrollMessage("Please Connect to AP: " + String(myWiFiManager->getConfigPortalSSID()));
  centerPrint("WiFi");
}

int getMinutesFromLastRefresh()
{
  return (now() - lastEpoch) / 60;
}

void performBrightnessAnimation()
{
  // Fade in
  for (int inx = 0; inx <= 15; inx++)
  {
    matrix.setIntensity(inx);
    delay(50);
  }
  // Fade out
  for (int inx = 15; inx >= 0; inx--)
  {
    matrix.setIntensity(inx);
    delay(50);
  }
  delay(500);
}

void setupWiFi()
{
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  String hostname = "Zegar TV";
  if (!wifiManager.autoConnect(hostname.c_str()))
  {
    Serial.println("Failed to connect to WiFi, restarting...");
    delay(3000);
    WiFi.disconnect(true);
    ESP.reset();
    delay(5000);
  }
  Serial.println("WiFi connected successfully");
}

// Remove unused checkDisplay function

void setup()
{
  Serial.begin(115200);
  delay(10);

  Serial.println("Number of LED Displays: " + String(NUMBER_OF_HORIZONTAL_DISPLAYS));
  matrix.setIntensity(0); // Start with brightness 0

  // Configure matrix panels
  int maxPos = NUMBER_OF_HORIZONTAL_DISPLAYS * NUMBER_OF_VERTICAL_DISPLAYS;
  for (int i = 0; i < maxPos; i++)
  {
    matrix.setRotation(i, LED_ROTATION);
    matrix.setPosition(i, maxPos - i - 1, 0);
  }

  Serial.println("Matrix initialized");
  matrix.fillScreen(LOW);
  centerPrint("Witaj");

  // Set hostname
  wifi_station_set_hostname(DEVICE_HOSTNAME.c_str());
  WiFi.hostname(DEVICE_HOSTNAME);

  // Brightness animation
  performBrightnessAnimation();
  matrix.setIntensity(DISPLAY_INTENSITY);

  // WiFi setup
  setupWiFi();
}

void loop()
{
  // Update time if needed
  if ((getMinutesFromLastRefresh() >= MINUTES_BETWEEN_DATA_REFRESH) || lastEpoch == 0)
  {
    updateTime();
  }

  // Update display when minute changes
  if (lastMinute != timeDB.zeroPad(minute()))
  {
    lastMinute = timeDB.zeroPad(minute());
  }

  // Display current time
  String currentTime = hourMinutes(false);
  matrix.fillScreen(LOW);
  centerPrint(currentTime);

  delay(100); // Small delay to prevent excessive CPU usage
}
