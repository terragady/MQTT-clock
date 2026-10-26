#include "TimeManager.h"
#include "Settings.h"
#include <time.h>

// The system clock starts at the 1970 epoch on boot. Anything later than this
// (2024-01-01) can only have come from an NTP sync.
const time_t MIN_SYNCED_EPOCH = 1704067200;

void TimeManager::initialize()
{
  configTime(TIMEZONE, NTP_SERVER);
  Serial.println(F("NTP sync started"));
}

String TimeManager::getFormattedTime()
{
  if (!isTimeSynced())
  {
    return F("--:--"); // Placeholder until the first successful time sync
  }

  tm now;
  localNow(now);
  const char separator = (FLASH_ON_SECONDS && (now.tm_sec % 2) == 0) ? ' ' : ':';

  char buffer[6]; // "HH:MM" + NUL
  snprintf(buffer, sizeof(buffer), "%d%c%02d", now.tm_hour, separator, now.tm_min);
  return String(buffer);
}

int TimeManager::minutesSinceMidnight()
{
  tm now;
  localNow(now);
  return now.tm_hour * 60 + now.tm_min;
}

bool TimeManager::isTimeSynced() const
{
  return time(nullptr) > MIN_SYNCED_EPOCH;
}

void TimeManager::localNow(tm &out)
{
  const time_t now = time(nullptr);
  localtime_r(&now, &out);
}
