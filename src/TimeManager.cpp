#include "TimeManager.h"
#include "Settings.h"
#include <TimeLib.h>

TimeManager::TimeManager(TimeDB &timeDBRef, DisplayManager &displayRef)
    : timeDB(timeDBRef), display(displayRef), timeSynced(false), lastSyncAttemptMs(0)
{
}

void TimeManager::updateTime()
{
  Serial.println("Updating Time...");

  // Record the attempt time up front so failures back off (see shouldUpdateTime)
  // instead of retrying every loop iteration.
  lastSyncAttemptMs = millis();

  // Show update indicator
  display.showUpdateIndicator();

  time_t currentTime = timeDB.getTime();
  if (currentTime == TimeDB::INVALID_TIME)
  {
    // Non-blocking failure: keep the clock running (the loop shows "--:--"
    // until the first sync) and simply retry later. No blocking error scroll.
    Serial.println("Time update failed! Will retry later.");
    return;
  }

  setTime(currentTime);
  timeSynced = true;
  Serial.println("Time updated successfully");
}

String TimeManager::getFormattedTime()
{
  if (!timeSynced)
  {
    return "--:--"; // Placeholder until the first successful time sync
  }
  return hourMinutes();
}

bool TimeManager::shouldUpdateTime()
{
  unsigned long nowMs = millis();

  // Very first attempt right after boot.
  if (lastSyncAttemptMs == 0)
  {
    return true;
  }

  // Before the first successful sync (e.g. no internet), retry on a short
  // interval rather than every loop, so the display stays responsive.
  if (!timeSynced)
  {
    return (nowMs - lastSyncAttemptMs) >= TIME_SYNC_RETRY_INTERVAL_MS;
  }

  // Once synced, refresh on the normal long interval.
  return (nowMs - lastSyncAttemptMs) >= ((unsigned long)MINUTES_BETWEEN_DATA_REFRESH * 60000UL);
}

String TimeManager::secondsIndicator()
{
  if (FLASH_ON_SECONDS && (second() % 2) == 0)
  {
    return " ";
  }
  return ":";
}

String TimeManager::hourMinutes()
{
  return String(hour()) + secondsIndicator() + timeDB.zeroPad(minute());
}
