#pragma once

enum {
  ovl_time_util_microseconds_per_second = 1000000ULL,
};

// Shared utility functions for time module
int ovl_time_util_days_from_civil(int const year, int const month, int const day);
void ovl_time_util_civil_from_days(int const days, int *const year, int *const month, int *const day);
