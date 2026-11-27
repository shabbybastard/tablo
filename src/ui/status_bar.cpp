#include "status_bar.h"

#include <cstdio>

#include "core/version.h"
#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {
void home_clicked_cb(lv_event_t*) { ui_go_home(); }
}  // namespace

void status_bar_create(StatusBar& bar, lv_obj_t* parent, const char* menu_text,
                       lv_event_cb_t menu_cb, lv_event_cb_t version_cb,
                       bool menu_right) {
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_add_style(root, &theme().status, 0);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_size(root, 480, 40);
  remove_flags(root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
  bar.root = root;

  bar.menu_button = lv_button_create(root);
  lv_obj_remove_style_all(bar.menu_button);
  lv_obj_add_style(bar.menu_button, &theme().button_ghost, 0);
  lv_obj_add_style(bar.menu_button, &theme().button_ghost_pressed,
                   LV_STATE_PRESSED);
  if (!menu_right) {
    lv_obj_t* home = lv_button_create(root);
    lv_obj_remove_style_all(home);
    lv_obj_add_style(home, &theme().button_ghost, 0);
    lv_obj_add_style(home, &theme().button_ghost_pressed, LV_STATE_PRESSED);
    lv_obj_set_pos(home, 6, 5);
    lv_obj_set_size(home, 40, 30);
    lv_obj_set_style_shadow_width(home, 0, 0);
    remove_flags(home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(home, home_clicked_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* home_label = make_label(home, LV_SYMBOL_HOME, nullptr);
    lv_obj_set_style_text_font(home_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(home_label, palette().accent, 0);
    lv_obj_set_size(home_label, 34, 26);
    lv_obj_set_style_text_align(home_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(home_label);
  }

  lv_obj_set_pos(bar.menu_button, menu_right ? 434 : 50, 5);
  lv_obj_set_size(bar.menu_button, 40, 30);
  lv_obj_set_style_shadow_width(bar.menu_button, 0, 0);
  remove_flags(bar.menu_button, LV_OBJ_FLAG_SCROLLABLE);
  if (menu_cb != nullptr) {
    lv_obj_add_event_cb(bar.menu_button, menu_cb, LV_EVENT_CLICKED, nullptr);
  }

  bar.menu_label = make_label(bar.menu_button, menu_text, &theme().accent18);
  lv_obj_set_size(bar.menu_label, 34, 26);
  lv_obj_set_style_text_align(bar.menu_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(bar.menu_label, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_center(bar.menu_label);

  const int32_t brand_x = menu_right ? 14 : 100;
  lv_obj_t* brand = make_label(root, "tablo", &theme().accent18);
  lv_obj_set_pos(brand, brand_x, 8);

  bar.version = make_label(root, "v" FW_VERSION, &theme().accent14);
  lv_obj_set_pos(bar.version, brand_x + 64, 10);
  lv_obj_set_size(bar.version, 110, 20);
  lv_obj_set_style_text_align(bar.version, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(bar.version, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_add_flag(bar.version, LV_OBJ_FLAG_CLICKABLE);
  if (version_cb != nullptr) {
    lv_obj_add_event_cb(bar.version, version_cb, LV_EVENT_CLICKED, nullptr);
  }

  make_divider(root, 0, 39, 480);
}

void status_bar_update(StatusBar& bar, const AppState& state) {
  const Palette& p = palette();
  char buffer[32];

  const char* version =
      state.fw_version[0] != '\0' ? state.fw_version : tablo::kFwVersion;
  std::snprintf(buffer, sizeof(buffer), "v%s", version);
  set_label_text(bar.version, buffer);

  const bool update_available = state.ota.available || state.ota.downloading;
  lv_obj_set_style_text_color(bar.version, update_available ? p.red : p.accent, 0);
  lv_obj_set_style_text_font(
      bar.version, update_available ? &ui_font_14_bold : &ui_font_14, 0);
}

}  // namespace ui
}  // namespace tablo
