#include "ui.h"

#include <lvgl.h>

#include "dashboard.h"
#include "fonts/fonts.h"
#include "options_screen.h"
#include "schedule_screen.h"
#include "sd_import_modal.h"
#include "settings_screen.h"
#include "setup_screen.h"
#include "theme.h"
#include "update_modal.h"
#include "wifi_screen.h"

namespace tablo {
namespace {

ui::Dashboard g_dashboard;
ui::SettingsScreen g_settings;
ui::OptionsScreen g_options;
ui::WifiScreen g_wifi;
ui::ScheduleScreen g_schedule;
ui::SetupScreen g_setup;
ui::UpdateModal g_modal;
ui::SdImportModal g_sd_modal;
AppState g_state;
bool g_ready = false;
bool g_auto_shown = false;
DisplayMode g_mode = DisplayMode::Day;
uint32_t g_sd_shown_event = 0;
bool g_setup_active = false;

const char* modal_version() {
  if (g_state.ota.version[0] != '\0') {
    return g_state.ota.version;
  }
  return "";
}

const char* modal_status() {
  if (g_state.ota.downloading) {
    return "Загрузка обновления...";
  }
  return "Доступна новая прошивка";
}

void version_clicked(lv_event_t*) {
#ifdef TABLO_PREVIEW
  const bool allowed = true;
#else
  const bool allowed = g_state.ota.available;
#endif
  if (!allowed) {
    return;
  }
  ui::update_modal_show(g_modal, modal_version(), modal_status());
}

void load_screen(lv_obj_t* target) {
  if (target == nullptr || target == lv_screen_active()) {
    return;
  }
  lv_screen_load_anim(target, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, false);
}

void menu_clicked(lv_event_t*) {
  lv_obj_t* active = lv_screen_active();
  if (active == g_settings.screen || active == g_schedule.screen) {
    load_screen(g_dashboard.screen);
  } else {
    load_screen(g_settings.screen);
  }
}

void schedule_clicked(lv_event_t*) { load_screen(g_schedule.screen); }

void schedule_back_cb(lv_event_t*) { load_screen(g_settings.screen); }

void options_clicked(lv_event_t*) { load_screen(g_options.screen); }

void options_back_cb(lv_event_t*) { load_screen(g_settings.screen); }

void wifi_clicked(lv_event_t*) {
  ui::wifi_refresh(g_wifi);
  load_screen(g_wifi.screen);
}

void wifi_back_cb(lv_event_t*) { load_screen(g_options.screen); }

void navigate_screen(lv_dir_t dir) {
  static uint32_t last_nav = 0;
  if (last_nav != 0 && lv_tick_elaps(last_nav) < 350U) {
    return;
  }
  lv_obj_t* active = lv_screen_active();
  lv_obj_t* target = nullptr;
  if (active == g_dashboard.screen) {
    target = g_settings.screen;
  } else if (active == g_settings.screen) {
    target = g_dashboard.screen;
  }
  if (target == nullptr || target == active) {
    return;
  }
  last_nav = lv_tick_get();
  const lv_screen_load_anim_t anim =
      dir == LV_DIR_LEFT ? LV_SCR_LOAD_ANIM_MOVE_LEFT
                         : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
  lv_screen_load_anim(target, anim, 260, 0, false);
}

lv_point_t g_swipe_start{};
bool g_swipe_active = false;

void swipe_pressed(lv_event_t*) {
  lv_indev_t* indev = lv_indev_active();
  if (indev != nullptr) {
    lv_indev_get_point(indev, &g_swipe_start);
  }
  g_swipe_active = true;
}

void swipe_pressing(lv_event_t*) {
  if (!g_swipe_active) {
    return;
  }
  lv_indev_t* indev = lv_indev_active();
  if (indev == nullptr) {
    return;
  }
  lv_point_t point{};
  lv_indev_get_point(indev, &point);
  const int32_t dx = point.x - g_swipe_start.x;
  const int32_t dy = point.y - g_swipe_start.y;
  if (LV_ABS(dx) > 60 && LV_ABS(dx) > LV_ABS(dy) * 3 / 2) {
    g_swipe_active = false;
    navigate_screen(dx < 0 ? LV_DIR_LEFT : LV_DIR_RIGHT);
  }
}

void swipe_released(lv_event_t*) { g_swipe_active = false; }

}  // namespace

void ui_init(bool setup_active) {
  if (g_ready) {
    return;
  }

  ui::theme_init();
  lv_display_t* display = lv_display_get_default();
  if (display != nullptr) {
    lv_theme_default_init(display, ui::palette().accent, ui::palette().green,
                          true, &ui_font_14);
  }

  ui::dashboard_create(g_dashboard, version_clicked, nullptr, menu_clicked);
  ui::settings_create(g_settings, version_clicked, nullptr, menu_clicked,
                      schedule_clicked, options_clicked);
  ui::options_create(g_options, version_clicked, options_back_cb, wifi_clicked);
  ui::wifi_create(g_wifi, version_clicked, wifi_back_cb);
  ui::schedule_screen_create(g_schedule, schedule_back_cb);
  ui::setup_screen_create(g_setup);
  ui::update_modal_create(g_modal);
  ui::sd_import_modal_create(g_sd_modal);
  ui::setup_screen_update(g_setup, g_state);

  g_setup_active = setup_active;
  if (g_dashboard.screen != nullptr) {
    lv_obj_add_event_cb(g_dashboard.screen, swipe_pressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(g_dashboard.screen, swipe_pressing, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(g_dashboard.screen, swipe_released, LV_EVENT_RELEASED, nullptr);
  }
  if (g_settings.screen != nullptr) {
    lv_obj_add_event_cb(g_settings.screen, swipe_pressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(g_settings.screen, swipe_pressing, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(g_settings.screen, swipe_released, LV_EVENT_RELEASED, nullptr);
  }

  if (setup_active && g_setup.screen != nullptr) {
    lv_screen_load(g_setup.screen);
  } else if (g_dashboard.screen != nullptr) {
    lv_screen_load(g_dashboard.screen);
  }
  g_ready = true;
}

void ui_update(const AppState& state) {
  if (!g_ready) {
    return;
  }
  g_state = state;

  if (g_state.display_mode != g_mode) {
    g_mode = g_state.display_mode;
    ui::theme_apply(g_mode);
  }

  ui::dashboard_update(g_dashboard, g_state);
  ui::settings_update(g_settings, g_state);
  ui::options_update(g_options, g_state);
  ui::wifi_update(g_wifi, g_state);
  ui::schedule_screen_update(g_schedule, g_state);
  ui::setup_screen_update(g_setup, g_state);
  ui::update_modal_update(g_modal, g_state);

  const bool setup_active = g_state.setup.active || g_setup_active;
  if (!setup_active) {
    if (!g_state.ota.downloading &&
        (g_state.sd_import.pending || g_state.sd_import.invalid) &&
        g_state.sd_import.event_id != 0 &&
        g_state.sd_import.event_id != g_sd_shown_event) {
      g_sd_shown_event = g_state.sd_import.event_id;
      ui::sd_import_modal_show(g_sd_modal, g_state.sd_import);
    } else {
      ui::sd_import_modal_update(g_sd_modal, g_state.sd_import);
    }
  } else if (g_sd_modal.visible) {
    ui::sd_import_modal_hide(g_sd_modal);
  }

#ifndef TABLO_PREVIEW
  if (!setup_active && !g_auto_shown && g_state.ota.available) {
    g_auto_shown = true;
    ui::update_modal_show(g_modal, modal_version(), modal_status());
  }
#endif
}

#ifdef TABLO_UI_TEST
void ui_test_show_dashboard() {
  if (g_ready && g_dashboard.screen != nullptr) {
    lv_screen_load(g_dashboard.screen);
  }
}

void ui_test_show_settings() {
  if (g_ready && g_settings.screen != nullptr) {
    lv_screen_load(g_settings.screen);
  }
}

void ui_test_show_schedule() {
  if (g_ready && g_schedule.screen != nullptr) {
    lv_screen_load(g_schedule.screen);
  }
}

void ui_test_show_options() {
  if (g_ready && g_options.screen != nullptr) {
    lv_screen_load(g_options.screen);
  }
}

void ui_test_show_wifi() {
  if (g_ready && g_wifi.screen != nullptr) {
    ui::wifi_refresh(g_wifi);
    lv_screen_load(g_wifi.screen);
  }
}

void ui_test_click_wifi_connect() {
  if (g_wifi.connect_button != nullptr) {
    lv_obj_send_event(g_wifi.connect_button, LV_EVENT_CLICKED, nullptr);
  }
}

bool ui_test_wifi_password_visible() {
  return g_wifi.password_modal != nullptr;
}

void ui_test_dismiss_wifi_password() { ui::wifi_hide_password(g_wifi); }

void ui_test_click_dashboard_menu() {
  if (g_dashboard.status.menu_button != nullptr) {
    lv_obj_send_event(g_dashboard.status.menu_button, LV_EVENT_CLICKED, nullptr);
  }
}

void ui_test_show_setup() {
  if (g_ready && g_setup.screen != nullptr) {
    lv_screen_load(g_setup.screen);
  }
}

void ui_test_click_setup_wifi() {
  if (g_setup.wifi_button != nullptr) {
    lv_obj_send_event(g_setup.wifi_button, LV_EVENT_CLICKED, nullptr);
  }
}

void ui_test_click_setup_sd() {
  if (g_setup.sd_button != nullptr) {
    lv_obj_send_event(g_setup.sd_button, LV_EVENT_CLICKED, nullptr);
  }
}

void ui_test_show_update_modal(const char* version, const char* status) {
  if (g_ready) {
    ui::update_modal_show(g_modal, version, status);
  }
}

void ui_test_hide_update_modal() { ui::update_modal_hide(g_modal); }

bool ui_test_update_modal_visible() { return g_modal.visible; }

bool ui_test_sd_import_modal_visible() { return g_sd_modal.visible; }

bool ui_test_version_highlighted() {
  if (g_dashboard.status.version == nullptr) {
    return false;
  }
  return lv_obj_get_style_text_font(g_dashboard.status.version, 0) ==
         &ui_font_14_bold;
}
#endif

}  // namespace tablo
