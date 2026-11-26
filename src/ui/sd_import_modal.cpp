#include "sd_import_modal.h"

#include "core/platform.h"
#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

void dismiss_cb(lv_event_t* event) {
  auto* modal = static_cast<SdImportModal*>(lv_event_get_user_data(event));
  if (modal != nullptr) {
    sd_import_modal_hide(*modal);
  }
  platform().requestAction(PlatformAction::SdImportDismiss);
}

void apply_cb(lv_event_t* event) {
  auto* modal = static_cast<SdImportModal*>(lv_event_get_user_data(event));
  if (modal == nullptr || modal->applying) {
    return;
  }
  modal->applying = true;
  lv_obj_add_state(modal->apply_button, LV_STATE_DISABLED);
  set_label_text(modal->status, "Применение настроек...");
  platform().requestAction(PlatformAction::SdImportApply);
}

}  // namespace

void sd_import_modal_create(SdImportModal& m) {
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
  lv_obj_set_size(m.panel, 400, 300);
  lv_obj_center(m.panel);
  remove_flags(m.panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(m.panel, LV_SCROLLBAR_MODE_OFF);

  m.icon = lv_obj_create(m.panel);
  lv_obj_remove_style_all(m.icon);
  lv_obj_set_size(m.icon, 48, 48);
  lv_obj_align(m.icon, LV_ALIGN_TOP_MID, 0, 16);
  lv_obj_set_style_radius(m.icon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(m.icon, palette().accent, 0);
  lv_obj_set_style_bg_opa(m.icon, LV_OPA_COVER, 0);
  remove_flags(m.icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  m.icon_label = make_label(m.icon, "SD", nullptr);
  lv_obj_set_style_text_font(m.icon_label, &ui_font_18, 0);
  lv_obj_set_style_text_color(m.icon_label, palette().dark_text, 0);
  lv_obj_center(m.icon_label);

  m.title = make_label(m.panel, "Настройки на microSD", &theme().text18);
  lv_obj_set_size(m.title, 360, 26);
  lv_obj_align(m.title, LV_ALIGN_TOP_MID, 0, 74);
  lv_obj_set_style_text_align(m.title, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(m.title, LV_LABEL_LONG_MODE_DOTS);

  m.file = make_label(m.panel, "tablo-config.json", &theme().accent14);
  lv_obj_set_size(m.file, 360, 20);
  lv_obj_align(m.file, LV_ALIGN_TOP_MID, 0, 104);
  lv_obj_set_style_text_align(m.file, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(m.file, LV_LABEL_LONG_MODE_DOTS);

  m.status = make_label(m.panel, "", &theme().muted14);
  lv_obj_set_size(m.status, 356, 66);
  lv_obj_align(m.status, LV_ALIGN_TOP_MID, 0, 132);
  lv_obj_set_style_text_align(m.status, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(m.status, LV_LABEL_LONG_MODE_WRAP);

  lv_obj_t* later_button = lv_button_create(m.panel);
  lv_obj_set_pos(later_button, 24, 230);
  lv_obj_set_size(later_button, 160, 44);
  lv_obj_add_style(later_button, &theme().button_ghost, 0);
  lv_obj_add_style(later_button, &theme().button_ghost_pressed, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(later_button, 0, 0);
  remove_flags(later_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(later_button, dismiss_cb, LV_EVENT_CLICKED, &m);

  m.later_label = make_label(later_button, "Позже", &theme().text18);
  lv_obj_set_size(m.later_label, 154, 24);
  lv_obj_set_style_text_align(m.later_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(m.later_label);

  m.apply_button = lv_button_create(m.panel);
  lv_obj_set_pos(m.apply_button, 216, 230);
  lv_obj_set_size(m.apply_button, 160, 44);
  lv_obj_add_style(m.apply_button, &theme().button_primary, 0);
  lv_obj_add_style(m.apply_button, &theme().button_primary_pressed,
                   LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(m.apply_button, 0, 0);
  remove_flags(m.apply_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(m.apply_button, apply_cb, LV_EVENT_CLICKED, &m);

  lv_obj_t* apply_label = make_label(m.apply_button, "Применить", nullptr);
  lv_obj_set_style_text_font(apply_label, &ui_font_18, 0);
  lv_obj_set_style_text_color(apply_label, palette().dark_text, 0);
  lv_obj_set_size(apply_label, 154, 24);
  lv_obj_set_style_text_align(apply_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(apply_label);
}

void sd_import_modal_show(SdImportModal& m, const SdImportInfo& info) {
  if (m.overlay == nullptr) {
    return;
  }
  m.applying = info.applying;
  m.invalid = info.invalid;
  m.event_id = info.event_id;

  if (info.invalid) {
    set_label_text(m.title, "Ошибка конфига");
    set_label_text(m.status,
                   info.error[0] != '\0' ? info.error : "Конфиг не прошёл проверку.");
    lv_obj_add_flag(m.apply_button, LV_OBJ_FLAG_HIDDEN);
    set_label_text(m.later_label, "Закрыть");
    lv_obj_remove_state(m.apply_button, LV_STATE_DISABLED);
  } else {
    set_label_text(m.title, "Настройки на microSD");
    set_label_text(m.status,
                   "Найден конфиг. Применить настройки и перезагрузить плату?");
    remove_flags(m.apply_button, LV_OBJ_FLAG_HIDDEN);
    set_label_text(m.later_label, "Позже");
    lv_obj_remove_state(m.apply_button, LV_STATE_DISABLED);
  }

  if (info.file[0] != '\0') {
    set_label_text_safe(m.file, info.file);
  } else {
    set_label_text(m.file, "tablo-config.json");
  }

  const Palette& p = palette();
  lv_obj_set_style_bg_color(m.icon, info.invalid ? p.amber : p.accent, 0);
  lv_obj_set_style_text_color(m.icon_label, p.dark_text, 0);

  remove_flags(m.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(m.overlay, -1);
  m.visible = true;
}

void sd_import_modal_hide(SdImportModal& m) {
  if (m.overlay == nullptr) {
    return;
  }
  lv_obj_add_flag(m.overlay, LV_OBJ_FLAG_HIDDEN);
  m.visible = false;
}

void sd_import_modal_update(SdImportModal& m, const SdImportInfo& info) {
  if (!m.visible || m.overlay == nullptr) {
    return;
  }
  if (!info.pending && !info.invalid) {
    sd_import_modal_hide(m);
    return;
  }
  if (info.applying && !m.applying) {
    m.applying = true;
    lv_obj_add_state(m.apply_button, LV_STATE_DISABLED);
    set_label_text(m.status, "Применение настроек...");
  }
}

}  // namespace ui
}  // namespace tablo
