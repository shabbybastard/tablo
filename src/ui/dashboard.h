#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"
#include "status_bar.h"

namespace tablo {
namespace ui {

struct Dashboard {
  lv_obj_t* screen = nullptr;
  StatusBar status;

  lv_obj_t* clock_label = nullptr;
  lv_obj_t* date_label = nullptr;
  lv_obj_t* note_label = nullptr;

  lv_obj_t* purifier_dot = nullptr;
  lv_obj_t* purifier_pm = nullptr;
  lv_obj_t* purifier_unit = nullptr;
  lv_obj_t* purifier_detail = nullptr;
  lv_obj_t* purifier_filter = nullptr;
  lv_obj_t* purifier_mode = nullptr;
  lv_obj_t* purifier_status = nullptr;
  int last_purifier_band = -1;
  bool last_purifier_online = false;
  bool last_purifier_enabled = false;
  bool last_purifier_configured = false;

  lv_obj_t* weather_temp = nullptr;
  lv_obj_t* weather_icon = nullptr;
  lv_obj_t* weather_condition = nullptr;
  lv_obj_t* weather_feels = nullptr;
  lv_obj_t* aqi_dot = nullptr;
  lv_obj_t* aqi_label = nullptr;
  lv_obj_t* pm_label = nullptr;
  lv_obj_t* aqi_quality = nullptr;
  bool last_weather_valid = false;
  int last_aqi = -1;

  DisplayMode last_mode = DisplayMode::Day;
};

void dashboard_create(Dashboard& dash, lv_event_cb_t version_cb,
                      lv_event_cb_t gesture_cb, lv_event_cb_t menu_cb);
void dashboard_update(Dashboard& dash, const AppState& state);

}  // namespace ui
}  // namespace tablo
