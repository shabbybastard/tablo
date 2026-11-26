#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"
#include "status_bar.h"

namespace tablo {
namespace ui {

struct WifiScreen {
  lv_obj_t* screen = nullptr;
  StatusBar status;
  lv_obj_t* current_label = nullptr;
  lv_obj_t* network_roller = nullptr;
  lv_obj_t* status_label = nullptr;
  lv_obj_t* connect_button = nullptr;
  lv_obj_t* footer = nullptr;

  uint32_t last_generation = 0;
  uint8_t last_count = 0;
  void* password_modal = nullptr;
};

void wifi_create(WifiScreen& wifi, lv_event_cb_t version_cb,
                 lv_event_cb_t menu_cb);
void wifi_update(WifiScreen& wifi, const AppState& state);
/** Ask the net worker for a fresh scan and show progress in the UI. */
void wifi_refresh(WifiScreen& wifi);
void wifi_hide_password(WifiScreen& wifi);

}  // namespace ui
}  // namespace tablo
