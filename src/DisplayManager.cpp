#include "DisplayManager.h"
#include "Settings.h"

extern int refresh; // Global refresh flag from main

DisplayManager::DisplayManager(Max72xxPanel &matrixRef) : matrix(matrixRef)
{
}

void DisplayManager::scrollMessage(String msg)
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

void DisplayManager::centerPrint(String msg)
{
  int x = calculateCenterX(msg.length());
  matrix.setCursor(x, 0);
  matrix.print(msg);
  matrix.write();
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
