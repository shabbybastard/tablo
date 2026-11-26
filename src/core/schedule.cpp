#include "schedule.h"

#include <cstdio>
#include <cstring>

namespace tablo {

bool night_schedule_valid(const NightSchedule& schedule) {
  if (schedule.day_min >= 24U * 60U || schedule.evening_min >= 24U * 60U ||
      schedule.night_min >= 24U * 60U) {
    return false;
  }
  return schedule.day_min < schedule.evening_min &&
         schedule.evening_min < schedule.night_min;
}

DisplayMode display_mode_for(const NightSchedule& schedule,
                             uint16_t minute_of_day, bool time_valid) {
  if (!time_valid || !schedule.enabled || !night_schedule_valid(schedule) ||
      minute_of_day >= 24U * 60U) {
    return DisplayMode::Day;
  }
  if (minute_of_day >= schedule.night_min || minute_of_day < schedule.day_min) {
    return DisplayMode::Night;
  }
  if (minute_of_day >= schedule.evening_min) {
    return DisplayMode::Evening;
  }
  return DisplayMode::Day;
}

uint8_t brightness_for_mode(uint8_t day_brightness, DisplayMode mode) {
  if (day_brightness > 100) {
    day_brightness = 100;
  }
  if (mode == DisplayMode::Evening) {
    const uint8_t value = static_cast<uint8_t>(
        static_cast<uint16_t>(day_brightness) * 60U / 100U);
    return value < 10 ? 10 : value;
  }
  if (mode == DisplayMode::Night) {
    const uint8_t value = static_cast<uint8_t>(
        static_cast<uint16_t>(day_brightness) * 25U / 100U);
    return value < 5 ? 5 : value;
  }
  return day_brightness;
}

bool parse_hhmm(const char* text, uint16_t& minutes) {
  if (text == nullptr) {
    return false;
  }
  while (*text == ' ' || *text == '\t') {
    ++text;
  }

  int hour = 0;
  int digits = 0;
  while (*text >= '0' && *text <= '9' && digits < 2) {
    hour = hour * 10 + (*text - '0');
    ++text;
    ++digits;
  }
  if (digits == 0 || *text != ':') {
    return false;
  }
  ++text;

  int minute = 0;
  digits = 0;
  while (*text >= '0' && *text <= '9' && digits < 2) {
    minute = minute * 10 + (*text - '0');
    ++text;
    ++digits;
  }
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  if (digits == 0 || *text != '\0' || hour > 23 || minute > 59) {
    return false;
  }
  minutes = static_cast<uint16_t>(hour * 60 + minute);
  return true;
}

void format_hhmm(uint16_t minutes, char* out, size_t cap) {
  if (out == nullptr || cap == 0) {
    return;
  }
  if (minutes >= 24U * 60U) {
    minutes = 0;
  }
  std::snprintf(out, cap, "%02u:%02u", static_cast<unsigned>(minutes / 60U),
                static_cast<unsigned>(minutes % 60U));
}

}  // namespace tablo
