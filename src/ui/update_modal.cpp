#include "update_modal.h"

#include <cstdio>

#include "core/platform.h"
#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

void later_cb(lv_event_t* event) {
  auto* modal = static_cast<UpdateModal*>(lv_event_get_user_data(event));
  if (modal != nullptr) {
    update_modal_hide(*modal);
  }
}

void install_cb(lv_event_t* event) {
  auto* modal = static_cast<UpdateModal*>(lv_event_get_user_data(event));
  if (modal == nullptr || modal->installing) {
    return;
  }
  modal->installing = true;
  platform().installUpdate();
  set_label_text(modal->status, "Загрузка обновления...");
  remove_flags(modal->progress, LV_OBJ_FLAG_HIDDEN);
  lv_bar_set_value(modal->progress, 0, LV_ANIM_OFF);
  lv_obj_add_state(modal->update_button, LV_STATE_DISABLED);
}

}  // namespace

void update_modal_create(UpdateModal& m) {
  m.overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(m.overlay);
  lv_obj_set_pos(m.overlay, 0, 0);
  lv_obj_set_size(m.overlay, 480, 480);
  lv_obj_set_style_bg_color(m.overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(m.overlay, LV_OPA_70, 0);
  lv_obj_add_flag(m.overlay, LV_OBJ_FLAG_CLICKABLE);
  remove_flags(m.overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(m.overlay, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(m.overlay, LV_OBJ_FLAG_HIDDEN);

  m.panel = lv_obj_create(m.overlay);
  lv_obj_remove_style_all(m.panel);
  lv_obj_add_style(m.panel, &theme().modal, 0);
  lv_obj_set_size(m.panel, 384, 268);
  lv_obj_center(m.panel);
  remove_flags(m.panel,
               LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(m.panel, LV_SCROLLBAR_MODE_OFF);

  m.icon = lv_obj_create(m.panel);
  lv_obj_remove_style_all(m.icon);
  lv_obj_set_size(m.icon, 48, 48);
  lv_obj_align(m.icon, LV_ALIGN_TOP_MID, 0, 16);
  lv_obj_set_style_radius(m.icon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(m.icon, palette().accent, 0);
  lv_obj_set_style_bg_opa(m.icon, LV_OPA_COVER, 0);
  remove_flags(m.icon,
               LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  m.icon_label = make_label(m.icon, "\xE2\x86\x93", nullptr);
  lv_obj_set_style_text_font(m.icon_label, &ui_font_28, 0);
  lv_obj_set_style_text_color(m.icon_label, palette().dark_text, 0);
  lv_obj_center(m.icon_label);

  m.title = make_label(m.panel, "Доступно обновление", &theme().text28);
  lv_obj_set_size(m.title, 360, 38);
  lv_obj_align(m.title, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_set_style_text_align(m.title, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(m.title, LV_LABEL_LONG_MODE_CLIP);

  m.version = make_label(m.panel, "Версия -", &theme().accent18);
  lv_obj_set_size(m.version, 340, 24);
  lv_obj_align(m.version, LV_ALIGN_TOP_MID, 0, 112);
  lv_obj_set_style_text_align(m.version, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(m.version, LV_LABEL_LONG_MODE_DOTS);

  m.status = make_label(m.panel, "Доступна новая прошивка",
                        &theme().muted14);
  lv_obj_set_size(m.status, 340, 20);
  lv_obj_align(m.status, LV_ALIGN_TOP_MID, 0, 140);
  lv_obj_set_style_text_align(m.status, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(m.status, LV_LABEL_LONG_MODE_DOTS);

  m.progress = lv_bar_create(m.panel);
  lv_obj_set_size(m.progress, 320, 6);
  lv_obj_align(m.progress, LV_ALIGN_TOP_MID, 0, 166);
  lv_bar_set_range(m.progress, 0, 100);
  lv_bar_set_value(m.progress, 0, LV_ANIM_OFF);
  lv_obj_add_style(m.progress, &theme().slider_track, LV_PART_MAIN);
  lv_obj_add_style(m.progress, &theme().slider_indicator, LV_PART_INDICATOR);
  remove_flags(m.progress,
               LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(m.progress, LV_OBJ_FLAG_HIDDEN);

  m.later_button = lv_button_create(m.panel);
  lv_obj_set_pos(m.later_button, 24, 198);
  lv_obj_set_size(m.later_button, 156, 44);
  lv_obj_add_style(m.later_button, &theme().button_ghost, 0);
  lv_obj_add_style(m.later_button, &theme().button_ghost_pressed,
                   LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(m.later_button, 0, 0);
  remove_flags(m.later_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(m.later_button, later_cb, LV_EVENT_CLICKED, &m);

  lv_obj_t* later_label = make_label(m.later_button, "Позже",
                                      &theme().text18);
  lv_obj_set_size(later_label, 150, 24);
  lv_obj_set_style_text_align(later_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(later_label);

  m.update_button = lv_button_create(m.panel);
  lv_obj_set_pos(m.update_button, 204, 198);
  lv_obj_set_size(m.update_button, 156, 44);
  lv_obj_add_style(m.update_button, &theme().button_primary, 0);
  lv_obj_add_style(m.update_button, &theme().button_primary_pressed,
                   LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(m.update_button, 0, 0);
  remove_flags(m.update_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(m.update_button, install_cb, LV_EVENT_CLICKED, &m);

  lv_obj_t* update_label = make_label(m.update_button, "Обновить", nullptr);
  lv_obj_set_style_text_font(update_label, &ui_font_18, 0);
  lv_obj_set_style_text_color(update_label, palette().dark_text, 0);
  lv_obj_set_size(update_label, 150, 24);
  lv_obj_set_style_text_align(update_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(update_label);
}

void update_modal_show(UpdateModal& m, const char* version,
                       const char* status) {
  if (m.overlay == nullptr) {
    return;
  }
  m.installing = false;
  lv_obj_remove_state(m.update_button, LV_STATE_DISABLED);
  lv_obj_add_flag(m.progress, LV_OBJ_FLAG_HIDDEN);
  lv_bar_set_value(m.progress, 0, LV_ANIM_OFF);

  char buffer[96];
  if (version != nullptr && version[0] != '\0') {
    std::snprintf(buffer, sizeof(buffer), "Версия %s", version);
  } else {
    std::snprintf(buffer, sizeof(buffer), "Версия -");
  }
  set_label_text(m.version, buffer);
  set_label_text_safe(m.status,
                      status != nullptr && status[0] != '\0'
                          ? status
                          : "Доступна новая прошивка");

  const Palette& p = palette();
  lv_obj_set_style_bg_color(m.icon, p.accent, 0);
  lv_obj_set_style_text_color(m.icon_label, p.dark_text, 0);
  m.last_mode = theme().mode;

  remove_flags(m.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(m.overlay, -1);
  m.visible = true;
}

void update_modal_hide(UpdateModal& m) {
  if (m.overlay == nullptr) {
    return;
  }
  lv_obj_add_flag(m.overlay, LV_OBJ_FLAG_HIDDEN);
  m.visible = false;
}

void update_modal_update(UpdateModal& m, const AppState& state) {
  if (!m.visible || m.overlay == nullptr) {
    return;
  }

  const Palette& p = palette();
  if (m.last_mode != theme().mode) {
    lv_obj_set_style_bg_color(m.icon, p.accent, 0);
    lv_obj_set_style_text_color(m.icon_label, p.dark_text, 0);
    m.last_mode = theme().mode;
  }

  if (state.ota.downloading) {
    m.installing = true;
    remove_flags(m.progress, LV_OBJ_FLAG_HIDDEN);
    int progress = state.ota.progress;
    if (progress < 0) {
      progress = 0;
    } else if (progress > 100) {
      progress = 100;
    }
    lv_bar_set_value(m.progress, progress, LV_ANIM_OFF);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "Загрузка обновления: %d%%",
                  progress);
    set_label_text(m.status, buffer);
    lv_obj_add_state(m.update_button, LV_STATE_DISABLED);
  } else if (m.installing) {
    if (!state.ota.available) {
      remove_flags(m.progress, LV_OBJ_FLAG_HIDDEN);
      lv_bar_set_value(m.progress, 100, LV_ANIM_OFF);
      if (state.ota.status[0] != '\0') {
        set_label_text_safe(m.status, state.ota.status);
      } else {
        set_label_text(m.status, "Обновление завершено");
      }
    }
  }
}

}  // namespace ui
}  // namespace tablo
