#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"
#include "status_bar.h"

namespace tablo {
namespace ui {

struct OptionsScreen {
  lv_obj_t* screen = nullptr;
  StatusBar status;

  lv_obj_t* timezone_value = nullptr;
  lv_obj_t* weather_switch = nullptr;
  lv_obj_t* date_format_value = nullptr;
  lv_obj_t* wifi_value = nullptr;
  lv_obj_t* footer = nullptr;

  int timezone_hours = 3;
  DateFormat date_format = DateFormat::DayMonthWeekday;
  bool last_weather_enabled = true;
  void* choice = nullptr;
};

void options_create(OptionsScreen& options, lv_event_cb_t version_cb,
                    lv_event_cb_t menu_cb, lv_event_cb_t wifi_cb);
void options_update(OptionsScreen& options, const AppState& state);

}  // namespace ui
}  // namespace tablo
