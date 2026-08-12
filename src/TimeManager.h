#pragma once
#include "Arduino.h"
#include "TimeDB.h"
#include "DisplayManager.h"

// How often to retry the time server before the first successful sync (ms).
// Prevents hammering the server every loop when there is no internet.
const unsigned long TIME_SYNC_RETRY_INTERVAL_MS = 30000UL;

class TimeManager
{
public:
  TimeManager(TimeDB &timeDBRef, DisplayManager &displayRef);

  // Time operations
  void updateTime();
  bool shouldUpdateTime();

  // Current time as "H:MM", or "--:--" before the first successful sync. The
  // colon blinks once a second when FLASH_ON_SECONDS is enabled.
  String getFormattedTime();

  bool isTimeSynced() const { return timeSynced; }

private:
  TimeDB &timeDB;
  DisplayManager &display;

  // Formatting helpers
  String secondsIndicator();
  String hourMinutes();

  // Sync state
  bool timeSynced;                 // true once we have a valid time at least once
  unsigned long lastSyncAttemptMs; // millis() of the last sync attempt (for backoff)
};
