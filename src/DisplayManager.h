#pragma once
#include "Arduino.h"
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

class DisplayManager
{
public:
  DisplayManager(Max72xxPanel &matrixRef);

  // Display operations
  void scrollMessage(String msg);
  void centerPrint(String msg);
  void performBrightnessAnimation();
  void showUpdateIndicator();
  void initializeMatrix();

  // Display configuration
  void setIntensity(int intensity);
  void fillScreen(bool state);
  void write();
  Max72xxPanel& getMatrix() { return matrix; }

private:
  Max72xxPanel &matrix;

  // Constants
  static const int FONT_WIDTH = 5;
  static const int SPACER = 1;
  static const int CHAR_WIDTH = FONT_WIDTH + SPACER;

  // Helper functions
  int calculateCenterX(int textLength);
};
