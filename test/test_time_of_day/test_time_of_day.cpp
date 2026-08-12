// Host-side tests for the hardware-independent time-of-day logic.
// Run with: pio test -e native

#include <unity.h>
#include "TimeOfDay.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------- formatting

void test_format_pads_hours_and_minutes(void)
{
  char buffer[9];
  TEST_ASSERT_TRUE(formatMinutesAsTime(7 * 60, buffer, sizeof(buffer)));
  TEST_ASSERT_EQUAL_STRING("07:00:00", buffer);

  TEST_ASSERT_TRUE(formatMinutesAsTime(22 * 60 + 5, buffer, sizeof(buffer)));
  TEST_ASSERT_EQUAL_STRING("22:05:00", buffer);
}

void test_format_handles_day_boundaries(void)
{
  char buffer[9];
  formatMinutesAsTime(0, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("00:00:00", buffer);

  formatMinutesAsTime(MINUTES_PER_DAY - 1, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("23:59:00", buffer);
}

void test_format_clamps_out_of_range(void)
{
  char buffer[9];
  formatMinutesAsTime(-1, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("00:00:00", buffer);

  formatMinutesAsTime(99999, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("23:59:00", buffer);
}

void test_format_rejects_small_buffer(void)
{
  char buffer[4];
  TEST_ASSERT_FALSE(formatMinutesAsTime(60, buffer, sizeof(buffer)));
  TEST_ASSERT_FALSE(formatMinutesAsTime(60, nullptr, 9));
}

// ------------------------------------------------------------------- parsing

void test_parse_accepts_hh_mm_and_hh_mm_ss(void)
{
  TEST_ASSERT_EQUAL_INT(7 * 60, parseTimeToMinutes("07:00"));
  TEST_ASSERT_EQUAL_INT(7 * 60, parseTimeToMinutes("07:00:00"));
  // Home Assistant time entities send HH:MM:SS; seconds are ignored.
  TEST_ASSERT_EQUAL_INT(22 * 60 + 30, parseTimeToMinutes("22:30:45"));
}

void test_parse_accepts_boundaries(void)
{
  TEST_ASSERT_EQUAL_INT(0, parseTimeToMinutes("00:00"));
  TEST_ASSERT_EQUAL_INT(MINUTES_PER_DAY - 1, parseTimeToMinutes("23:59"));
  TEST_ASSERT_EQUAL_INT(9 * 60 + 5, parseTimeToMinutes("9:5"));
}

void test_parse_rejects_out_of_range(void)
{
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("24:00"));
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("23:60"));
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("99:99"));
}

void test_parse_rejects_malformed_input(void)
{
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes(nullptr));
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes(""));
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("nonsense"));
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("1230"));   // no separator
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("12:"));    // empty minute
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("12:30:")); // empty second
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("ab:30"));  // non-numeric hour
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes("12:cd"));  // non-numeric minute
}

// This is the regression that motivated the rewrite: String::toInt() reads an
// empty hour field as 0, so ":30" used to be accepted as 00:30.
void test_parse_rejects_empty_hour_field(void)
{
  TEST_ASSERT_EQUAL_INT(-1, parseTimeToMinutes(":30"));
}

void test_parse_round_trips_with_format(void)
{
  char buffer[9];
  for (int minutes = 0; minutes < MINUTES_PER_DAY; minutes += 17)
  {
    formatMinutesAsTime(minutes, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(minutes, parseTimeToMinutes(buffer));
  }
}

// ------------------------------------------------------------- day/night window

void test_day_window_normal_order(void)
{
  const int dayStart = 7 * 60;    // 07:00
  const int nightStart = 22 * 60; // 22:00

  TEST_ASSERT_TRUE(isWithinDayWindow(7 * 60, dayStart, nightStart));      // inclusive start
  TEST_ASSERT_TRUE(isWithinDayWindow(12 * 60, dayStart, nightStart));     // midday
  TEST_ASSERT_TRUE(isWithinDayWindow(21 * 60 + 59, dayStart, nightStart));
  TEST_ASSERT_FALSE(isWithinDayWindow(22 * 60, dayStart, nightStart));    // exclusive end
  TEST_ASSERT_FALSE(isWithinDayWindow(6 * 60 + 59, dayStart, nightStart));
  TEST_ASSERT_FALSE(isWithinDayWindow(0, dayStart, nightStart));          // midnight
}

void test_day_window_wraps_past_midnight(void)
{
  // Day runs 22:00 -> 06:00, i.e. the day window crosses midnight.
  const int dayStart = 22 * 60;
  const int nightStart = 6 * 60;

  TEST_ASSERT_TRUE(isWithinDayWindow(23 * 60, dayStart, nightStart));
  TEST_ASSERT_TRUE(isWithinDayWindow(0, dayStart, nightStart));
  TEST_ASSERT_TRUE(isWithinDayWindow(5 * 60 + 59, dayStart, nightStart));
  TEST_ASSERT_FALSE(isWithinDayWindow(6 * 60, dayStart, nightStart));
  TEST_ASSERT_FALSE(isWithinDayWindow(12 * 60, dayStart, nightStart));
}

// Equal boundaries leave no night window. Treated as always-day so a bad
// schedule cannot leave the panel stuck at night brightness.
void test_day_window_equal_boundaries_is_always_day(void)
{
  TEST_ASSERT_TRUE(isWithinDayWindow(0, 8 * 60, 8 * 60));
  TEST_ASSERT_TRUE(isWithinDayWindow(8 * 60, 8 * 60, 8 * 60));
  TEST_ASSERT_TRUE(isWithinDayWindow(23 * 60, 8 * 60, 8 * 60));
}

int main(int, char **)
{
  UNITY_BEGIN();

  RUN_TEST(test_format_pads_hours_and_minutes);
  RUN_TEST(test_format_handles_day_boundaries);
  RUN_TEST(test_format_clamps_out_of_range);
  RUN_TEST(test_format_rejects_small_buffer);

  RUN_TEST(test_parse_accepts_hh_mm_and_hh_mm_ss);
  RUN_TEST(test_parse_accepts_boundaries);
  RUN_TEST(test_parse_rejects_out_of_range);
  RUN_TEST(test_parse_rejects_malformed_input);
  RUN_TEST(test_parse_rejects_empty_hour_field);
  RUN_TEST(test_parse_round_trips_with_format);

  RUN_TEST(test_day_window_normal_order);
  RUN_TEST(test_day_window_wraps_past_midnight);
  RUN_TEST(test_day_window_equal_boundaries_is_always_day);

  return UNITY_END();
}
