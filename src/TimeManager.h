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
  String getFormattedTime(bool isRefresh = false);
  int getMinutesFromLastRefresh();
  bool shouldUpdateTime();
  bool hasMinuteChanged();

  // Time formatting helpers
  String secondsIndicator(bool isRefresh);
  String hourMinutes(bool isRefresh);

  // Getters
  String getLastMinute() const { return lastMinute; }
  long getLastEpoch() const { return lastEpoch; }
  long getFirstEpoch() const { return firstEpoch; }
  bool isTimeSynced() const { return timeSynced; }

private:
  TimeDB &timeDB;
  DisplayManager &display;

  // Time tracking variables
  String lastMinute;
  long lastEpoch;
  long firstEpoch;

  // Sync state
  bool timeSynced;                  // true once we have a valid time at least once
  unsigned long lastSyncAttemptMs;  // millis() of the last sync attempt (for backoff)
};
