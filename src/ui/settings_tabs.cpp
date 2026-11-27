#include "settings_tabs.h"

#include <cstdint>

#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

void tab_cb(lv_event_t* event) {
  const int tab = static_cast<int>(
      reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  ui_show_settings_tab(tab);
}

}  // namespace

void settings_tabs_create(lv_obj_t* parent, int active) {
  lv_obj_t* bar = make_card(parent, 12, 48, 456, 40);
  static const char* const kLabels[4] = {"Основные", "Локальные",
                                         "Устройство", "Внешние"};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t* button = lv_button_create(bar);
    lv_obj_remove_style_all(button);
    if (i == active) {
      lv_obj_add_style(button, &theme().button_primary, 0);
    } else {
      lv_obj_add_style(button, &theme().button_ghost, 0);
      lv_obj_add_style(button, &theme().button_ghost_pressed,
                       LV_STATE_PRESSED);
    }
    lv_obj_set_pos(button, 4 + i * 113, 4);
    lv_obj_set_size(button, 109, 32);
    lv_obj_set_style_shadow_width(button, 0, 0);
    remove_flags(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, tab_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(i)));

    lv_obj_t* label = make_label(button, kLabels[i], nullptr);
    lv_obj_set_style_text_font(label, &ui_font_14, 0);
    lv_obj_set_style_text_color(
        label, i == active ? palette().dark_text : palette().accent, 0);
    lv_obj_set_size(label, 105, 20);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
  }
}

}  // namespace ui
}  // namespace tablo
