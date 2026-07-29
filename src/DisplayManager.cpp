#include "DisplayManager.h"
#include "Settings.h"
#include "BackgroundService.h"

extern int refresh; // Global refresh flag from main

DisplayManager::DisplayManager(Max72xxPanel &matrixRef) : matrix(matrixRef)
{
}

void DisplayManager::scrollMessage(const String &msg)
{
  scrollMessage(msg, DISPLAY_SCROLL_SPEED); // Use default speed
}

void DisplayManager::scrollMessage(const String &msg, int speed)
{
  String scrollMsg = sanitizeText(msg) + " "; // add a space at the end
  for (int i = 0; i < (int)(CHAR_WIDTH * scrollMsg.length() + matrix.width() - 1 - SPACER); i++)
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
      if (letter < (int)scrollMsg.length())
      {
        matrix.drawChar(x, y, scrollMsg[letter], HIGH, LOW, 1);
      }
      letter--;
      x -= CHAR_WIDTH;
    }

    matrix.write();       // Send bitmap to display
    serviceDelay(speed);  // Wait per-step while keeping background services alive
  }
  matrix.setCursor(0, 0);
}

void DisplayManager::centerPrint(const String &msg)
{
  String text = sanitizeText(msg);
  int x = calculateCenterX(text.length());
  matrix.setCursor(x, 0);
  matrix.print(text);
  matrix.write();
}

void DisplayManager::fadeMessage(int targetBrightness, int stepDelayMs)
{
  targetBrightness = constrain(targetBrightness, 0, 15);

  // Fade out
  for (int level = targetBrightness; level >= 0; level--)
  {
    matrix.setIntensity(level);
    delay(stepDelayMs);
  }
  // Fade back in
  for (int level = 0; level <= targetBrightness; level++)
  {
    matrix.setIntensity(level);
    delay(stepDelayMs);
  }
}

void DisplayManager::performBrightnessAnimation()
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

void DisplayManager::showUpdateIndicator()
{
  matrix.drawPixel(0, 4, HIGH);
  matrix.drawPixel(0, 3, HIGH);
  matrix.drawPixel(0, 2, HIGH);
  matrix.write();
}

void DisplayManager::initializeMatrix()
{
  Serial.println("Number of LED Displays: " + String(NUMBER_OF_HORIZONTAL_DISPLAYS));
  matrix.setIntensity(0); // Start with brightness 0

  // Use real CP437 mapping so high-range glyphs (e.g. the degree sign at
  // 0xF8) render correctly. Without this, Adafruit GFX shifts every byte
  // >= 0xB0 by one and prints the wrong character.
  matrix.cp437(true);

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
}

void DisplayManager::setIntensity(int intensity)
{
  matrix.setIntensity(intensity);
}

void DisplayManager::fillScreen(bool state)
{
  matrix.fillScreen(state);
}

void DisplayManager::write()
{
  matrix.write();
}

int DisplayManager::calculateCenterX(int textLength)
{
  return (matrix.width() - (textLength * CHAR_WIDTH)) / 2;
}

String DisplayManager::sanitizeText(const String &msg)
{
  String out;
  out.reserve(msg.length());

  for (unsigned int i = 0; i < msg.length(); i++)
  {
    unsigned char c = (unsigned char)msg[i];

    // UTF-8 degree sign (U+00B0) arrives as the two bytes 0xC2 0xB0.
    if (c == 0xC2 && (i + 1) < msg.length() && (unsigned char)msg[i + 1] == 0xB0)
    {
      out += (char)0xF8; // CP437 degree glyph
      i++;               // skip the trailing 0xB0
      continue;
    }

    // Bare Latin-1 degree byte, just in case a client sends it un-encoded.
    if (c == 0xB0)
    {
      out += (char)0xF8;
      continue;
    }

    out += (char)c;
  }

  return out;
}
