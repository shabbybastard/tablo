#pragma once

#include <lvgl.h>

#include "core/app_state.h"
#include "status_bar.h"

namespace tablo {
namespace ui {

struct ExternalScreen {
  lv_obj_t* screen = nullptr;
  StatusBar status;

  lv_obj_t* enable_switch = nullptr;
  lv_obj_t* status_dot = nullptr;
  lv_obj_t* status_label = nullptr;
  lv_obj_t* ip_summary = nullptr;
  lv_obj_t* token_summary = nullptr;
  lv_obj_t* model_label = nullptr;
  lv_obj_t* updated_label = nullptr;
  lv_obj_t* check_button = nullptr;
  lv_obj_t* footer = nullptr;

  void* modal = nullptr;
  bool syncing_switch = false;
};

void external_screen_create(ExternalScreen& screen, lv_event_cb_t version_cb,
                            lv_event_cb_t menu_cb);
void external_screen_update(ExternalScreen& screen, const AppState& state);

void external_hide_modal(ExternalScreen& screen);
bool external_modal_visible(const ExternalScreen& screen);
void external_modal_set_text(ExternalScreen& screen, const char* text);
void external_modal_save(ExternalScreen& screen);
void external_edit_ip(ExternalScreen& screen);
void external_edit_token(ExternalScreen& screen);

}  // namespace ui
}  // namespace tablo
