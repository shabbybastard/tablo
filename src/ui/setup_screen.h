#pragma once

#include <lvgl.h>

#include "core/app_state.h"

namespace tablo {
namespace ui {

struct SetupScreen {
  lv_obj_t* screen = nullptr;
  lv_obj_t* brand = nullptr;
  lv_obj_t* version = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* subtitle = nullptr;

  lv_obj_t* wifi_button = nullptr;
  lv_obj_t* sd_button = nullptr;

  lv_obj_t* info_card = nullptr;
  lv_obj_t* info_title = nullptr;
  lv_obj_t* info_value = nullptr;
  lv_obj_t* info_line1 = nullptr;
  lv_obj_t* info_line2 = nullptr;

  lv_obj_t* status = nullptr;
  lv_obj_t* hint = nullptr;
  lv_obj_t* back_button = nullptr;
};

void setup_screen_create(SetupScreen& screen);
void setup_screen_update(SetupScreen& screen, const AppState& state);

}  // namespace ui
}  // namespace tablo
