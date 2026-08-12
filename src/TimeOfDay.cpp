#include "TimeOfDay.h"
#include <stdio.h>

namespace
{

int clampToDay(int minutes)
{
  if (minutes < 0)
  {
    return 0;
  }
  if (minutes > MINUTES_PER_DAY - 1)
  {
    return MINUTES_PER_DAY - 1;
  }
  return minutes;
}

// Reads digits until `terminator` (or NUL). Returns -1 when the field is empty
// or holds anything other than digits, so that a value like "ab:30" or ":30" is
// rejected instead of silently parsing as 0.
int readNumericField(const char *start, char terminator, const char **fieldEnd)
{
  int value = 0;
  int digitCount = 0;

  const char *cursor = start;
  while (*cursor != '\0' && *cursor != terminator)
  {
    if (*cursor < '0' || *cursor > '9')
    {
      return -1;
    }
    // Guard against absurdly long digit runs overflowing the accumulator.
    if (digitCount > 4)
    {
      return -1;
    }
    value = value * 10 + (*cursor - '0');
    digitCount++;
    cursor++;
  }

  if (digitCount == 0)
  {
    return -1;
  }

  *fieldEnd = cursor;
  return value;
}

} // namespace

bool formatMinutesAsTime(int minutes, char *buffer, unsigned int bufferSize)
{
  if (buffer == nullptr || bufferSize < 9)
  {
    return false;
  }

  const int clamped = clampToDay(minutes);
  snprintf(buffer, bufferSize, "%02d:%02d:00", clamped / 60, clamped % 60);
  return true;
}

int parseTimeToMinutes(const char *value)
{
  if (value == nullptr)
  {
    return -1;
  }

  const char *afterHour = nullptr;
  const int hour = readNumericField(value, ':', &afterHour);
  if (hour < 0 || *afterHour != ':')
  {
    return -1; // no colon at all, or a bad hour field
  }

  const char *afterMinute = nullptr;
  const int minute = readNumericField(afterHour + 1, ':', &afterMinute);
  if (minute < 0)
  {
    return -1;
  }

  // Anything after the minute field must be a seconds field, which we accept
  // but ignore. A trailing colon with nothing after it is malformed.
  if (*afterMinute == ':')
  {
    const char *afterSecond = nullptr;
    if (readNumericField(afterMinute + 1, '\0', &afterSecond) < 0)
    {
      return -1;
    }
  }

  if (hour > 23 || minute > 59)
  {
    return -1;
  }

  return hour * 60 + minute;
}

bool isWithinDayWindow(int currentMinutes, int dayStartMinutes, int nightStartMinutes)
{
  if (dayStartMinutes == nightStartMinutes)
  {
    return true;
  }

  if (dayStartMinutes < nightStartMinutes)
  {
    return currentMinutes >= dayStartMinutes && currentMinutes < nightStartMinutes;
  }

  return currentMinutes >= dayStartMinutes || currentMinutes < nightStartMinutes;
}
