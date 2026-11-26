#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"

namespace tablo {
namespace ui {

struct StatusBar {
  lv_obj_t* root = nullptr;
  lv_obj_t* menu_button = nullptr;
  lv_obj_t* menu_label = nullptr;
  lv_obj_t* wifi_icon = nullptr;
  lv_obj_t* wifi_arc[3] = {};
  lv_obj_t* wifi_dot = nullptr;
  lv_obj_t* version = nullptr;

  bool last_wifi = false;
  DisplayMode last_mode = DisplayMode::Day;
};

void status_bar_create(StatusBar& bar, lv_obj_t* parent, const char* menu_text,
                       lv_event_cb_t menu_cb, lv_event_cb_t version_cb);
void status_bar_update(StatusBar& bar, const AppState& state);

}  // namespace ui
}  // namespace tablo
