#pragma once

#include <cstddef>
#include <cstdint>

namespace tablo {

enum class DateFormat : uint8_t {
  DayMonthWeekday = 0,
  WeekdayDayMonth = 1,
  Numeric = 2,
};

/** Human-readable sample for the settings row. */
const char* date_format_label(DateFormat format);

/**
 * Format the dashboard date. `month` is 1..12, `weekday` is 0..6 (Sunday = 0).
 */
void format_date_text(char* out, size_t cap, int day, int month, int year,
                      int weekday, DateFormat format);

}  // namespace tablo
