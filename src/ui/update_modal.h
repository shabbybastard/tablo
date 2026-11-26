#pragma once

#include <lvgl.h>

#include "core/app_state.h"

namespace tablo {
namespace ui {

struct UpdateModal {
  lv_obj_t* overlay = nullptr;
  lv_obj_t* panel = nullptr;
  lv_obj_t* icon = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* version = nullptr;
  lv_obj_t* status = nullptr;
  lv_obj_t* progress = nullptr;
  lv_obj_t* update_button = nullptr;
  lv_obj_t* later_button = nullptr;
  bool visible = false;
  bool installing = false;
  DisplayMode last_mode = DisplayMode::Day;
};

void update_modal_create(UpdateModal& modal);
void update_modal_show(UpdateModal& modal, const char* version,
                       const char* status);
void update_modal_hide(UpdateModal& modal);
void update_modal_update(UpdateModal& modal, const AppState& state);

}  // namespace ui
}  // namespace tablo
