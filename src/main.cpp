#include "Arduino.h"
#include "Settings.h"
#define CONFIG "/conf.txt"

int refresh = 0;
int spacer = 1;         // dots between letters
int width = 5 + spacer; // The font width is 5 pixels + spacer
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

// Time
TimeDB TimeDB("");
String lastMinute = "xx";
int displayRefreshCount = 1;
long lastEpoch = 0;
long firstEpoch = 0;
long displayOffEpoch = 0;
boolean displayOn = true;

const int TIMEOUT = 500; // 500 = 1/2 second
int timeoutCount = 0;

String secondsIndicator(boolean isRefresh)
{
  String rtnValue = ":";
  if (isRefresh == false && (flashOnSeconds && (second() % 2) == 0))
  {
    rtnValue = " ";
  }
  return rtnValue;
}
String hourMinutes(boolean isRefresh)
{
  return String(hour()) + secondsIndicator(isRefresh) + TimeDB.zeroPad(minute());
}

void scrollMessage(String msg)
{
  msg += " "; // add a space at the end
  for (int i = 0; i < width * msg.length() + matrix.width() - 1 - spacer; i++)
  {
    if (refresh == 1)
      i = 0;
    refresh = 0;
    matrix.fillScreen(LOW);

    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // center the text vertically

    while (x + width - spacer >= 0 && letter >= 0)
    {
      if (letter < msg.length())
      {
        matrix.drawChar(x, y, msg[letter], HIGH, LOW, 1);
      }

      letter--;
      x -= width;
    }

    matrix.write(); // Send bitmap to display
    delay(displayScrollSpeed);
  }
  matrix.setCursor(0, 0);
}

void updateTime() //client function to send/receive GET request data.
{
  Serial.println("Updating Time...");
  //Update the Time
  matrix.drawPixel(0, 4, HIGH);
  matrix.drawPixel(0, 3, HIGH);
  matrix.drawPixel(0, 2, HIGH);
  matrix.write();
  time_t currentTime = TimeDB.getTime();
  if (currentTime > 5000)
  {
    setTime(currentTime);
  }
  else
  {
    scrollMessage("Time update failed!");
    scrollMessage("Time update failed!");
  }
  lastEpoch = now();
  if (firstEpoch == 0)
  {
    firstEpoch = now();
  }
}

void centerPrint(String msg)
{
  int x = (matrix.width() - (msg.length() * width)) / 2;
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
  int minutes = (now() - lastEpoch) / 60;
  return minutes;
}

void checkDisplay()
{
  String currentTime = TimeDB.zeroPad(hour()) + ":" + TimeDB.zeroPad(minute());
}

void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println("Number of LED Displays: " + String(numberOfHorizontalDisplays));
  matrix.setIntensity(0); // Use a value between 0 and 15 for brightness

  int maxPos = numberOfHorizontalDisplays * numberOfVerticalDisplays;
  for (int i = 0; i < maxPos; i++)
  {
    matrix.setRotation(i, ledRotation);
    matrix.setPosition(i, maxPos - i - 1, 0);
  }

  Serial.println("matrix created");
  matrix.fillScreen(LOW); // show black
  centerPrint("Witaj");
  wifi_station_set_hostname ("ZegarTV");
  WiFi.hostname("ZegarTV");

  for (int inx = 0; inx <= 15; inx++)
  {
    matrix.setIntensity(inx);
    delay(50);
  }
  for (int inx = 15; inx >= 0; inx--)
  {
    matrix.setIntensity(inx);
    delay(50);
  }
  delay(500);
  matrix.setIntensity(displayIntensity);

  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  String hostname(' ');
  hostname += String("Zegar TV");
  if (!wifiManager.autoConnect((const char *)hostname.c_str()))
  { // new addition
    delay(3000);
    WiFi.disconnect(true);
    ESP.reset();
    delay(5000);
    Serial.println("This should never happen");
  }
}

void loop()
{
  if ((getMinutesFromLastRefresh() >= minutesBetweenDataRefresh) || lastEpoch == 0)
  {
    updateTime();
  }
  if (lastMinute != TimeDB.zeroPad(minute()))
  {
    lastMinute = TimeDB.zeroPad(minute());
  }
  String currentTime = hourMinutes(false);
  matrix.fillScreen(LOW);
  centerPrint(currentTime);
}