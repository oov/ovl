#include <ovl/time.h>

#include "time_util.h"

static bool parse_2digits(char const *const str, int *const value) {
  if (!str || !value) {
    return false;
  }
  unsigned int const d0 = (unsigned int)(str[0] - '0');
  unsigned int const d1 = (unsigned int)(str[1] - '0');
  if (d0 > 9U || d1 > 9U) {
    return false;
  }
  *value = (int)(d0 * 10U + d1);
  return true;
}

static bool parse_4digits(char const *const str, int *const value) {
  if (!str || !value) {
    return false;
  }
  unsigned int const d0 = (unsigned int)(str[0] - '0');
  unsigned int const d1 = (unsigned int)(str[1] - '0');
  unsigned int const d2 = (unsigned int)(str[2] - '0');
  unsigned int const d3 = (unsigned int)(str[3] - '0');
  if (d0 > 9U || d1 > 9U || d2 > 9U || d3 > 9U) {
    return false;
  }
  *value = (int)(d0 * 1000U + d1 * 100U + d2 * 10U + d3);
  return true;
}

static int const month_lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

bool ovl_time_parse(char const *const str,
                    size_t const len,
                    uint64_t *const timestamp_us,
                    int32_t *const tz_offset_sec) {
  if (!str || !timestamp_us) {
    return false;
  }

  char const *pos = str;
  char const *const end = str + len;
  if ((size_t)(end - pos) < 10) {
    return false;
  }

  int year = 0;
  if ((size_t)(end - pos) < 4 || !parse_4digits(pos, &year)) {
    return false;
  }
  pos += 4;

  if (pos == end || pos[0] != '-') {
    return false;
  }
  pos++;

  int month = 0;
  if ((size_t)(end - pos) < 2 || !parse_2digits(pos, &month)) {
    return false;
  }
  pos += 2;

  if (pos == end || pos[0] != '-') {
    return false;
  }
  pos++;

  int day = 0;
  if ((size_t)(end - pos) < 2 || !parse_2digits(pos, &day)) {
    return false;
  }
  pos += 2;

  int hour = 0;
  int minute = 0;
  int second = 0;
  int microsecond = 0;

  if (pos != end && pos[0] == 'T') {
    pos++;
    if ((size_t)(end - pos) < 2 || !parse_2digits(pos, &hour)) {
      return false;
    }
    pos += 2;

    if (pos != end && pos[0] == ':') {
      pos++;
      if ((size_t)(end - pos) < 2 || !parse_2digits(pos, &minute)) {
        return false;
      }
      pos += 2;

      if (pos != end && pos[0] == ':') {
        pos++;
        if ((size_t)(end - pos) < 2 || !parse_2digits(pos, &second)) {
          return false;
        }
        pos += 2;

        if (pos != end && pos[0] == '.') {
          pos++;
          if (pos == end || (unsigned int)(pos[0] - '0') > 9U) {
            return false;
          }

          int digits = 0;
          microsecond = 0;
          while (pos != end && (unsigned int)(pos[0] - '0') <= 9U && digits < 6) {
            microsecond = microsecond * 10 + (pos[0] - '0');
            pos++;
            digits++;
          }
          while (digits < 6) {
            microsecond *= 10;
            digits++;
          }
          while (pos != end && (unsigned int)(pos[0] - '0') <= 9U) {
            pos++;
          }
        }
      }
    }
  }

  int tz_offset_seconds = 0;
  if (pos != end) {
    char const tz_char = pos[0];
    if (tz_char == 'Z' || tz_char == 'z') {
      pos++;
    } else if ((tz_char == '+' || tz_char == '-') && (size_t)(end - pos) >= 3) {
      int const sign = (tz_char == '+') ? 1 : -1;
      pos++;

      int tz_hours = 0;
      if ((size_t)(end - pos) < 2 || !parse_2digits(pos, &tz_hours)) {
        return false;
      }
      pos += 2;

      int tz_minutes = 0;
      if (pos != end) {
        if ((size_t)(end - pos) < 3 || pos[0] != ':') {
          return false;
        }
        pos++;
        if ((size_t)(end - pos) < 2 || !parse_2digits(pos, &tz_minutes)) {
          return false;
        }
        pos += 2;
      }

      if (tz_hours < 0 || tz_hours > 23 || tz_minutes < 0 || tz_minutes > 59) {
        return false;
      }
      tz_offset_seconds = sign * (tz_hours * 3600 + tz_minutes * 60);
    } else {
      return false;
    }
  }

  if (pos != end) {
    return false;
  }

  int const *const month_lengths_table = month_lengths;
  bool const is_leap_year = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  int const max_day = month_lengths_table[month - 1] + ((month == 2 && is_leap_year) ? 1 : 0);

  if (year < 1970 || year > 9999 || month < 1 || month > 12 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
      second < 0 || second > 59 || microsecond < 0 || microsecond > 999999 || day < 1 || day > max_day) {
    return false;
  }

  int const days = ovl_time_util_days_from_civil(year, month, day);
  uint64_t total_seconds = (uint64_t)days * 24ULL * 3600ULL + (uint64_t)(hour * 3600 + minute * 60 + second);

  if (tz_offset_seconds >= 0) {
    total_seconds -= (uint64_t)tz_offset_seconds;
  } else {
    total_seconds += (uint64_t)(-tz_offset_seconds);
  }

  if (tz_offset_sec) {
    *tz_offset_sec = tz_offset_seconds;
  }

  *timestamp_us = total_seconds * ovl_time_util_microseconds_per_second + (uint64_t)microsecond;
  return true;
}
