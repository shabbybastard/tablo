#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"
#include "status_bar.h"

namespace tablo {
namespace ui {

struct SystemScreen {
  lv_obj_t* screen = nullptr;
  StatusBar status;

  lv_obj_t* ip_value = nullptr;
  lv_obj_t* wifi_value = nullptr;
  lv_obj_t* cpu_value = nullptr;
  lv_obj_t* ram_value = nullptr;
  lv_obj_t* flash_value = nullptr;
  lv_obj_t* uptime_value = nullptr;
  lv_obj_t* footer = nullptr;
  lv_obj_t* check_button = nullptr;
  lv_obj_t* reboot_button = nullptr;
};

void system_screen_create(SystemScreen& screen, lv_event_cb_t version_cb,
                          lv_event_cb_t menu_cb);
void system_screen_update(SystemScreen& screen, const AppState& state);

}  // namespace ui
}  // namespace tablo
