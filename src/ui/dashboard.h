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
  lv_obj_t* wifi_icon_card = nullptr;
  lv_obj_t* wifi_label = nullptr;
  lv_obj_t* ota_label = nullptr;
  bool last_wifi = false;

  lv_obj_t* weather_temp = nullptr;
  lv_obj_t* weather_condition = nullptr;
  lv_obj_t* weather_feels = nullptr;
  lv_obj_t* aqi_dot = nullptr;
  lv_obj_t* aqi_label = nullptr;
  lv_obj_t* pm_label = nullptr;
  lv_obj_t* aqi_quality = nullptr;
  bool last_weather_valid = false;
  int last_aqi = -1;

  lv_obj_t* net_ip = nullptr;
  lv_obj_t* ping_pill[3] = {};
  lv_obj_t* ping_name[3] = {};
  lv_obj_t* ping_label[3] = {};
  int last_ping_state[3] = {-1, -1, -1};

  lv_obj_t* note_label = nullptr;
  lv_obj_t* uptime_label = nullptr;
  lv_obj_t* note_ip = nullptr;

  DisplayMode last_mode = DisplayMode::Day;
  int last_ota_kind = -1;
};

void dashboard_create(Dashboard& dash, lv_event_cb_t version_cb,
                      lv_event_cb_t gesture_cb, lv_event_cb_t menu_cb);
void dashboard_update(Dashboard& dash, const AppState& state);

}  // namespace ui
}  // namespace tablo
