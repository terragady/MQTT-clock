#include "TimeManager.h"
#include "Settings.h"
#include <TimeLib.h>

TimeManager::TimeManager(TimeDB &timeDBRef, DisplayManager &displayRef)
    : timeDB(timeDBRef), display(displayRef), lastMinute("xx"), lastEpoch(0), firstEpoch(0), timeoutCount(0)
{
}

void TimeManager::updateTime()
{
  Serial.println("Updating Time...");

  // Show update indicator
  display.showUpdateIndicator();

  time_t currentTime = timeDB.getTime();
  if (currentTime > 5000)
  {
    setTime(currentTime);
    Serial.println("Time updated successfully");
  }
  else
  {
    Serial.println("Time update failed!");
    display.scrollMessage("Time update failed!");
    return; // Don't update lastEpoch if time update failed
  }

  lastEpoch = now();
  if (firstEpoch == 0)
  {
    firstEpoch = now();
  }
}

String TimeManager::getFormattedTime(bool isRefresh)
{
  return hourMinutes(isRefresh);
}

int TimeManager::getMinutesFromLastRefresh()
{
  return (now() - lastEpoch) / 60;
}

bool TimeManager::shouldUpdateTime()
{
  return (getMinutesFromLastRefresh() >= MINUTES_BETWEEN_DATA_REFRESH) || lastEpoch == 0;
}

bool TimeManager::hasMinuteChanged()
{
  String currentMinute = timeDB.zeroPad(minute());
  if (lastMinute != currentMinute)
  {
    lastMinute = currentMinute;
    return true;
  }
  return false;
}

String TimeManager::secondsIndicator(bool isRefresh)
{
  String rtnValue = ":";
  if (!isRefresh && FLASH_ON_SECONDS && (second() % 2) == 0)
  {
    rtnValue = " ";
  }
  return rtnValue;
}

String TimeManager::hourMinutes(bool isRefresh)
{
  return String(hour()) + secondsIndicator(isRefresh) + timeDB.zeroPad(minute());
}
