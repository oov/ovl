#include "time_util.h"

// These helpers are adapted from Howard Hinnant's algorithms:
// https://howardhinnant.github.io/date_algorithms.html

int ovl_time_util_days_from_civil(int const year, int const month, int const day) {
  int const adjusted_year = year - (month <= 2);
  int const era = (adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
  int const yoe = adjusted_year - era * 400;
  int const mp = month + (month > 2 ? -3 : 9);
  int const doy = (153 * mp + 2) / 5 + day - 1;
  int const doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void ovl_time_util_civil_from_days(int const days, int *const year, int *const month, int *const day) {
  int const z = days + 719468; // Offset to the algorithm's epoch (0000-03-01)
  int const era = (z >= 0 ? z : z - 146096) / 146097;
  int const doe = z - era * 146097;                                      // [0, 146096]
  int const yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
  int const yoe_year = yoe + era * 400;
  int const doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
  int const mp = (5 * doy + 2) / 153;                      // [0, 11]
  int const day_local = doy - (153 * mp + 2) / 5 + 1;
  int const month_local = mp + (mp < 10 ? 3 : -9);
  int const year_local = yoe_year + (month_local <= 2);
  if (year) {
    *year = year_local;
  }
  if (month) {
    *month = month_local;
  }
  if (day) {
    *day = day_local;
  }
}
