#pragma once

#include <cstddef>
#include <cstdint>

namespace tablo {

/** Visual mode selected by the wall-clock schedule or the manual switch. */
enum class DisplayMode : uint8_t {
  Day = 0,
  Evening,
  Night,
};

struct NightSchedule {
  bool enabled = false;
  uint16_t evening_min = 18U * 60U;  // 18:00
  uint16_t night_min = 23U * 60U;    // 23:00
  uint16_t day_min = 7U * 60U;       // 07:00
};

/** True when day < evening < night and all values are valid minutes. */
bool night_schedule_valid(const NightSchedule& schedule);

/**
 * Day/evening/night for a local wall-clock minute. Returns Day when the
 * schedule is disabled, invalid or the clock is not valid yet.
 */
DisplayMode display_mode_for(const NightSchedule& schedule,
                             uint16_t minute_of_day, bool time_valid);

/** Effective backlight for a mode. Evening/night scale the day brightness. */
uint8_t brightness_for_mode(uint8_t day_brightness, DisplayMode mode);

/** "HH:MM" / "H:MM" parser and formatter (minutes since midnight). */
bool parse_hhmm(const char* text, uint16_t& minutes);
void format_hhmm(uint16_t minutes, char* out, size_t cap);

}  // namespace tablo
