#pragma once

#include "core/app_state.h"

namespace tablo {

void ui_init(bool setup_active = false);
void ui_update(const AppState& state);

#ifdef TABLO_UI_TEST
// Deterministic screen switching for the headless native smoke test. These
// helpers are compiled only when tests/ui defines TABLO_UI_TEST.
void ui_test_show_dashboard();
void ui_test_show_settings();
void ui_test_show_schedule();
void ui_test_show_options();
void ui_test_show_wifi();
void ui_test_click_wifi_connect();
bool ui_test_wifi_password_visible();
void ui_test_dismiss_wifi_password();
void ui_test_click_dashboard_menu();
void ui_test_show_setup();
void ui_test_click_setup_wifi();
void ui_test_click_setup_sd();
void ui_test_show_update_modal(const char* version, const char* status);
void ui_test_hide_update_modal();
bool ui_test_update_modal_visible();
bool ui_test_sd_import_modal_visible();
bool ui_test_version_highlighted();
#endif

}  // namespace tablo
