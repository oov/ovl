#include <ovl/time.h>

#include "time_util.h"

static void to_2digit(int const value, char buf[2]) {
  buf[0] = (char)('0' + (value / 10));
  buf[1] = (char)('0' + (value % 10));
}

static void to_4digit(int const value, char buf[4]) {
  buf[0] = (char)('0' + (value / 1000));
  buf[1] = (char)('0' + ((value / 100) % 10));
  buf[2] = (char)('0' + ((value / 10) % 10));
  buf[3] = (char)('0' + (value % 10));
}

char *ovl_time_format(uint64_t const timestamp_us, char buf[26], int32_t const tz_offset_sec) {
  if (!buf) {
    return NULL;
  }

  static uint64_t const max_supported_seconds = 253402300799ULL; // 9999-12-31T23:59:59
  uint64_t total_seconds = timestamp_us / ovl_time_util_microseconds_per_second;
  if (total_seconds > max_supported_seconds) {
    total_seconds = max_supported_seconds;
  }

  static uint64_t const seconds_per_day = 60 * 60 * 24;
  int const days = (int)(total_seconds / seconds_per_day);
  int const seconds_today = (int)(total_seconds % seconds_per_day);

  static int const seconds_per_hour = 60 * 60;
  int const hour = seconds_today / seconds_per_hour;
  int const seconds_this_hour = seconds_today % seconds_per_hour;

  static int const seconds_per_minute = 60;
  int const minute = seconds_this_hour / seconds_per_minute;
  int const second = seconds_this_hour % seconds_per_minute;

  int year = 0;
  int month = 0;
  int day = 0;
  ovl_time_util_civil_from_days(days, &year, &month, &day);

  // Format base: YYYY-MM-DDTHH:MM:SS
  // Position: 0123456789...
  //           YYYY-MM-DDTHH:MM:SS
  to_4digit(year, buf + 0);
  buf[4] = '-';
  to_2digit(month, buf + 5);
  buf[7] = '-';
  to_2digit(day, buf + 8);
  buf[10] = 'T';
  to_2digit(hour, buf + 11);
  buf[13] = ':';
  to_2digit(minute, buf + 14);
  buf[16] = ':';
  to_2digit(second, buf + 17);

  // Format timezone
  if (tz_offset_sec == 0) {
    buf[19] = 'Z';
    buf[20] = '\0';
  } else {
    // Format: +HH:MM or -HH:MM
    int32_t offset = tz_offset_sec;
    char const sign = (offset > 0) ? '+' : '-';
    if (offset < 0) {
      offset = -offset;
    }

    int const tz_hours = (int)(offset / 3600);
    int const tz_minutes = (int)((offset % 3600) / 60);

    buf[19] = sign;
    to_2digit(tz_hours, buf + 20);
    buf[22] = ':';
    to_2digit(tz_minutes, buf + 23);
    buf[25] = '\0';
  }

  return buf;
}
