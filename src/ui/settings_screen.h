#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"
#include "status_bar.h"

namespace tablo {
namespace ui {

struct SettingsScreen {
  lv_obj_t* screen = nullptr;
  StatusBar status;

  lv_obj_t* brightness_value = nullptr;
  lv_obj_t* slider = nullptr;
  lv_obj_t* night_switch = nullptr;
  lv_obj_t* schedule_button = nullptr;
  lv_obj_t* schedule_summary = nullptr;
  lv_obj_t* footer = nullptr;

  bool brightness_override = false;
  uint8_t brightness_desired = 80;
  uint32_t brightness_until = 0;
  bool syncing_slider = false;

  bool night_override = false;
  bool night_desired = false;
  uint32_t night_until = 0;
  bool syncing_switch = false;
};

void settings_create(SettingsScreen& settings, lv_event_cb_t version_cb,
                     lv_event_cb_t gesture_cb, lv_event_cb_t menu_cb,
                     lv_event_cb_t schedule_cb);
void settings_update(SettingsScreen& settings, const AppState& state);

}  // namespace ui
}  // namespace tablo
