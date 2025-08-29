#pragma once
#include "Arduino.h"
#include "TimeDB.h"
#include "DisplayManager.h"

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

private:
  TimeDB &timeDB;
  DisplayManager &display;

  // Time tracking variables
  String lastMinute;
  long lastEpoch;
  long firstEpoch;
  int timeoutCount;
};
