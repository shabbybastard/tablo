#include "status_bar.h"

#include <cstdio>

#include "core/version.h"
#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {

void status_bar_create(StatusBar& bar, lv_obj_t* parent, const char* menu_text,
                       lv_event_cb_t menu_cb, lv_event_cb_t version_cb) {
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_add_style(root, &theme().status, 0);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_size(root, 480, 40);
  remove_flags(root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
  bar.root = root;

  lv_obj_t* brand_dot = lv_obj_create(root);
  lv_obj_remove_style_all(brand_dot);
  lv_obj_add_style(brand_dot, &theme().dot_active, 0);
  lv_obj_set_pos(brand_dot, 14, 16);
  lv_obj_set_size(brand_dot, 8, 8);
  remove_flags(brand_dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* brand = make_label(root, "tablo", &theme().accent18);
  lv_obj_set_pos(brand, 28, 8);

  bar.power = make_label(root, "", &theme().muted14);
  lv_obj_set_pos(bar.power, 78, 10);
  lv_obj_set_size(bar.power, 100, 20);
  lv_label_set_long_mode(bar.power, LV_LABEL_LONG_MODE_DOTS);

  bar.menu_button = lv_button_create(root);
  lv_obj_remove_style_all(bar.menu_button);
  lv_obj_add_style(bar.menu_button, &theme().button_ghost, 0);
  lv_obj_add_style(bar.menu_button, &theme().button_ghost_pressed,
                   LV_STATE_PRESSED);
  lv_obj_set_pos(bar.menu_button, 184, 6);
  lv_obj_set_size(bar.menu_button, 110, 28);
  lv_obj_set_style_shadow_width(bar.menu_button, 0, 0);
  remove_flags(bar.menu_button, LV_OBJ_FLAG_SCROLLABLE);
  if (menu_cb != nullptr) {
    lv_obj_add_event_cb(bar.menu_button, menu_cb, LV_EVENT_CLICKED, nullptr);
  }

  bar.menu_label = make_label(bar.menu_button, menu_text, &theme().text14);
  lv_obj_set_size(bar.menu_label, 104, 20);
  lv_obj_set_style_text_align(bar.menu_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(bar.menu_label, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_center(bar.menu_label);

  /* Wi-Fi icon is drawn with three arcs and a dot: the built-in Montserrat
   * fonts used for the clock do not contain the Wi-Fi FontAwesome glyph. */
  bar.wifi_icon = lv_obj_create(root);
  lv_obj_remove_style_all(bar.wifi_icon);
  lv_obj_set_pos(bar.wifi_icon, 318, 9);
  lv_obj_set_size(bar.wifi_icon, 16, 20);
  remove_flags(bar.wifi_icon,
               LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  static const int32_t kArcSize[3] = {16, 10, 6};
  static const int32_t kArcX[3] = {0, 3, 5};
  static const int32_t kArcY[3] = {6, 9, 11};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t* arc = lv_arc_create(bar.wifi_icon);
    lv_obj_set_pos(arc, kArcX[i], kArcY[i]);
    lv_obj_set_size(arc, kArcSize[i], kArcSize[i]);
    remove_flags(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_arc_set_angles(arc, 225, 315);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    set_pad_all(arc, 0);
    lv_obj_set_style_arc_color(arc, palette().muted, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(arc, 0, LV_PART_KNOB);
    bar.wifi_arc[i] = arc;
  }
  bar.wifi_dot = make_dot(bar.wifi_icon, 6, 16, 4, palette().muted);

  bar.version = make_label(root, "v" FW_VERSION, &theme().accent14);
  lv_obj_set_pos(bar.version, 410, 10);
  lv_obj_set_size(bar.version, 58, 20);
  lv_obj_set_style_text_align(bar.version, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(bar.version, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_add_flag(bar.version, LV_OBJ_FLAG_CLICKABLE);
  if (version_cb != nullptr) {
    lv_obj_add_event_cb(bar.version, version_cb, LV_EVENT_CLICKED, nullptr);
  }

  make_divider(root, 0, 39, 480);
}

void status_bar_update(StatusBar& bar, const AppState& state) {
  const Palette& p = palette();
  const bool mode_changed = bar.last_mode != theme().mode;

  if (mode_changed || state.wifi_connected != bar.last_wifi) {
    const lv_color_t wifi_color = state.wifi_connected ? p.green : p.muted;
    for (int i = 0; i < 3; ++i) {
      lv_obj_set_style_arc_color(bar.wifi_arc[i], wifi_color,
                                 LV_PART_INDICATOR);
    }
    lv_obj_set_style_bg_color(bar.wifi_dot, wifi_color, 0);
    bar.last_wifi = state.wifi_connected;
  }

  char buffer[32];
  if (state.power.valid) {
    std::snprintf(buffer, sizeof(buffer), "%.1fV %.1fW",
                  static_cast<double>(state.power.voltage_v),
                  static_cast<double>(state.power.power_w));
  } else {
    buffer[0] = '\0';
  }
  set_label_text(bar.power, buffer);

  const char* version =
      state.fw_version[0] != '\0' ? state.fw_version : tablo::kFwVersion;
  std::snprintf(buffer, sizeof(buffer), "v%s", version);
  set_label_text(bar.version, buffer);

  const bool update_available = state.ota.available || state.ota.downloading;
  lv_obj_set_style_text_color(bar.version, update_available ? p.red : p.accent, 0);
  lv_obj_set_style_text_font(
      bar.version, update_available ? &ui_font_14_bold : &ui_font_14, 0);

  bar.last_mode = theme().mode;
}

}  // namespace ui
}  // namespace tablo
