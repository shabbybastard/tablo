#include "core/date_format.h"

#include <cstdio>

namespace tablo {
namespace {
const char* const kMonths[] = {"янв", "фев", "мар", "апр", "мая", "июн",
                               "июл", "авг", "сен", "окт", "ноя", "дек"};
const char* const kWeekdays[] = {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
}  // namespace

const char* date_format_label(DateFormat format) {
  switch (format) {
    case DateFormat::WeekdayDayMonth:
      return "Чт, 26 ноя";
    case DateFormat::Numeric:
      return "26.11.2026";
    case DateFormat::DayMonthWeekday:
    default:
      return "26 ноя · Чт";
  }
}

void format_date_text(char* out, size_t cap, int day, int month, int year,
                      int weekday, DateFormat format) {
  if (out == nullptr || cap == 0) {
    return;
  }
  const int month_index = month >= 1 && month <= 12 ? month - 1 : 0;
  const int weekday_index = weekday >= 0 && weekday <= 6 ? weekday : 0;
  switch (format) {
    case DateFormat::WeekdayDayMonth:
      std::snprintf(out, cap, "%s, %d %s", kWeekdays[weekday_index], day,
                    kMonths[month_index]);
      break;
    case DateFormat::Numeric:
      std::snprintf(out, cap, "%02d.%02d.%04d", day, month, year);
      break;
    case DateFormat::DayMonthWeekday:
    default:
      std::snprintf(out, cap, "%d %s · %s", day, kMonths[month_index],
                    kWeekdays[weekday_index]);
      break;
  }
}

}  // namespace tablo
