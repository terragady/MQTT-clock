#include "DisplayManager.h"
#include "Settings.h"
#include "BackgroundService.h"

DisplayManager::DisplayManager(Max72xxPanel &matrixRef) : matrix(matrixRef), lastIntensity(-1)
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
  const String text = sanitizeText(msg);
  // Text wider than the panel would otherwise centre to a negative x and get
  // clipped at both ends; starting at the left edge at least keeps the
  // beginning readable. Callers that can scroll should check textFits() first.
  int x = calculateCenterX(text.length());
  if (x < 0)
  {
    x = 0;
  }
  matrix.setCursor(x, 0);
  matrix.print(text);
  matrix.write();
}

bool DisplayManager::textFits(const String &msg)
{
  return (int)(sanitizeText(msg).length() * CHAR_WIDTH) <= matrix.width();
}

void DisplayManager::fadeMessage(int targetBrightness, int stepDelayMs)
{
  targetBrightness = constrain(targetBrightness, 0, 15);

  // Go through setIntensity() rather than the panel directly so the cached
  // intensity stays in step with what the hardware actually holds.
  // Fade out
  for (int level = targetBrightness; level >= 0; level--)
  {
    setIntensity(level);
    serviceDelay(stepDelayMs);
  }
  // Fade back in
  for (int level = 0; level <= targetBrightness; level++)
  {
    setIntensity(level);
    serviceDelay(stepDelayMs);
  }
}

void DisplayManager::performBrightnessAnimation()
{
  // Fade in
  for (int inx = 0; inx <= 15; inx++)
  {
    setIntensity(inx);
    serviceDelay(50);
  }
  // Fade out
  for (int inx = 15; inx >= 0; inx--)
  {
    setIntensity(inx);
    serviceDelay(50);
  }
  serviceDelay(500);
}

void DisplayManager::initializeMatrix()
{
  Serial.print(F("Number of LED Displays: "));
  Serial.println(NUMBER_OF_HORIZONTAL_DISPLAYS);
  setIntensity(0); // Start with brightness 0

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

  Serial.println(F("Matrix initialized"));
  matrix.fillScreen(LOW);
  centerPrint("Witaj");
}

void DisplayManager::setIntensity(int intensity)
{
  const int clamped = constrain(intensity, 0, 15);
  if (clamped == lastIntensity)
  {
    return;
  }
  lastIntensity = clamped;
  matrix.setIntensity(clamped);
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
