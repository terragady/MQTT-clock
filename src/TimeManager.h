#pragma once
#include "Arduino.h"
#include <time.h>

class TimeManager
{
public:
  // Start background NTP sync. Non-blocking: the core's SNTP client resyncs on
  // its own (hourly) and the POSIX TZ rule applies DST at the exact moment it
  // changes, so nothing needs to be polled from loop().
  void initialize();

  // Current time as "H:MM", or "--:--" before the first successful sync. The
  // colon blinks once a second when FLASH_ON_SECONDS is enabled.
  String getFormattedTime();

  // Local time of day as minutes since midnight (0-1439). Only meaningful once
  // isTimeSynced() is true.
  int minutesSinceMidnight();

  bool isTimeSynced() const;

private:
  // Fills `out` with the current local time.
  void localNow(tm &out);
};
