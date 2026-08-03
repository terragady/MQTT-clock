#pragma once
#include "Arduino.h"
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

class DisplayManager
{
public:
  DisplayManager(Max72xxPanel &matrixRef);

  // Display operations
  void scrollMessage(const String &msg);
  void scrollMessage(const String &msg, int speed); // Overloaded version with custom speed
  void centerPrint(const String &msg);
  // Fade the currently displayed content out (to 0) and back in (to
  // targetBrightness), stepDelayMs per intensity step. The content is not
  // redrawn, only the panel intensity is modulated.
  void fadeMessage(int targetBrightness, int stepDelayMs);
  void performBrightnessAnimation();
  void showUpdateIndicator();
  void initializeMatrix();

  // Display configuration
  void setIntensity(int intensity);
  void fillScreen(bool state);
  void write();
  Max72xxPanel &getMatrix() { return matrix; }

private:
  Max72xxPanel &matrix;

  // Constants
  static const int FONT_WIDTH = 5;
  static const int SPACER = 1;
  static const int CHAR_WIDTH = FONT_WIDTH + SPACER;

  // Helper functions
  int calculateCenterX(int textLength);

  // Convert UTF-8 payloads (e.g. from MQTT) into the single-byte CP437 codes
  // the LED font uses. Currently maps the degree sign; extend as needed.
  String sanitizeText(const String &msg);
};
