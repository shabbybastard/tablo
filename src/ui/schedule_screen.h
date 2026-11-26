#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"
#include "core/schedule.h"
#include "time_picker.h"

namespace tablo {
namespace ui {

enum class ScheduleField : uint8_t {
  Evening = 0,
  Night,
  Day,
};

/** Screen with the wall-clock evening/night/day schedule. */
struct ScheduleScreen {
  lv_obj_t* screen = nullptr;
  lv_obj_t* auto_switch = nullptr;
  lv_obj_t* evening_row = nullptr;
  lv_obj_t* night_row = nullptr;
  lv_obj_t* day_row = nullptr;
  lv_obj_t* evening_value = nullptr;
  lv_obj_t* night_value = nullptr;
  lv_obj_t* day_value = nullptr;
  lv_obj_t* status = nullptr;
  lv_obj_t* warning = nullptr;
  lv_obj_t* back_button = nullptr;

  TimePicker picker;
  ScheduleField editing = ScheduleField::Evening;

  /** Last effective schedule (includes pending local edits). */
  NightSchedule schedule;

  bool syncing_switch = false;

  bool auto_override = false;
  bool auto_desired = false;
  uint32_t auto_until = 0;

  bool values_override = false;
  NightSchedule values_desired;
  uint32_t values_until = 0;

  DisplayMode last_mode = DisplayMode::Day;
};

void schedule_screen_create(ScheduleScreen& screen, lv_event_cb_t back_cb);
void schedule_screen_update(ScheduleScreen& screen, const AppState& state);

}  // namespace ui
}  // namespace tablo
