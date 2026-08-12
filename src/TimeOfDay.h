#pragma once

// Pure time-of-day helpers. Deliberately free of Arduino types (no String, no
// hardware access) so the same code that runs on the device can be unit tested
// on the host - see test/test_time_of_day.
//
// Schedule values are "minutes since midnight" in the range 0-1439.

const int MINUTES_PER_DAY = 24 * 60;

// Formats minutes-since-midnight as "HH:MM:SS" (the format Home Assistant's
// time entities expect). buffer must hold at least 9 bytes. Out-of-range input
// is clamped. Returns false only when the buffer is too small.
bool formatMinutesAsTime(int minutes, char *buffer, unsigned int bufferSize);

// Parses "HH:MM" or "HH:MM:SS" into minutes since midnight.
// Returns -1 for anything malformed: no colon, an empty or non-numeric field,
// hour > 23 or minute > 59. Seconds are accepted but ignored.
int parseTimeToMinutes(const char *value);

// True when currentMinutes falls inside the day window. Handles the wrap-around
// case (night start before day start) and the degenerate case where both
// boundaries are equal, which is treated as always-day so the display is never
// stuck at night brightness.
bool isWithinDayWindow(int currentMinutes, int dayStartMinutes, int nightStartMinutes);
