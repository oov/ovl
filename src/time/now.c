#include <ovl/time.h>

#include "time_util.h"

#include <time.h>

uint64_t ovl_time_now(void) {
  struct timespec ts = {0};
  timespec_get(&ts, TIME_UTC);

  uint64_t sec = (uint64_t)ts.tv_sec;
  uint64_t us = (uint64_t)ts.tv_nsec / 1000;
  return sec * ovl_time_util_microseconds_per_second + us;
}
