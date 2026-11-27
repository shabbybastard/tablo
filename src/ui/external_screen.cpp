#include "external_screen.h"

#include <cstdio>

#include <cstdio>
#include <cstring>

#include "core/miio.h"
#include "core/platform.h"
#include "core/state_store.h"
#include "fonts/fonts.h"
#include "settings_tabs.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

enum class EditField { Ip, Token };

struct TextEditModal {
  ExternalScreen* screen = nullptr;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* textarea = nullptr;
  lv_obj_t* hint = nullptr;
  EditField field = EditField::Ip;
};

void make_primary_button(lv_obj_t* button) {
  lv_obj_add_style(button, &theme().button_primary, 0);
  lv_obj_add_style(button, &theme().button_primary_pressed, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(button, 0, 0);
  remove_flags(button, LV_OBJ_FLAG_SCROLLABLE);
}

void make_ghost_button(lv_obj_t* button) {
  lv_obj_add_style(button, &theme().button_ghost, 0);
  lv_obj_add_style(button, &theme().button_ghost_pressed, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(button, 0, 0);
  remove_flags(button, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* make_edit_row(lv_obj_t* parent, int32_t y, const char* caption,
                        lv_event_cb_t callback, ExternalScreen* screen,
                        lv_obj_t** summary) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  make_ghost_button(button);
  lv_obj_set_pos(button, 20, y);
  lv_obj_set_size(button, 416, 42);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, screen);

  lv_obj_t* label = make_label(button, caption, &theme().text18);
  lv_obj_set_pos(label, 10, 9);
  lv_obj_set_size(label, 150, 26);

  *summary = make_label(button, "", &theme().muted14);
  lv_obj_set_pos(*summary, 150, 11);
  lv_obj_set_size(*summary, 236, 22);
  lv_obj_set_style_text_align(*summary, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(*summary, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* arrow = make_label(button, ">", &theme().accent14);
  lv_obj_set_pos(arrow, 386, 10);
  lv_obj_set_size(arrow, 20, 22);
  lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_RIGHT, 0);
  return button;
}

void close_modal(TextEditModal* modal) {
  if (modal == nullptr) return;
  if (modal->screen != nullptr) {
    modal->screen->modal = nullptr;
  }
  if (modal->overlay != nullptr) {
    lv_obj_delete(modal->overlay);
  }
  delete modal;
}

void modal_cancel_cb(lv_event_t* event) {
  close_modal(static_cast<TextEditModal*>(lv_event_get_user_data(event)));
}

void save_modal(TextEditModal* modal) {
  if (modal == nullptr || modal->textarea == nullptr ||
      modal->screen == nullptr) {
    return;
  }

  char text[40] = {};
  std::snprintf(text, sizeof(text), "%s",
                lv_textarea_get_text(modal->textarea));
  const ExternalInfo external = g_state.snapshot().external;

  if (modal->field == EditField::Ip) {
    if (text[0] != '\0' && !miio_valid_ipv4(text)) {
      lv_label_set_text(modal->hint, "Неверный IPv4, пример: 192.0.2.50");
      lv_obj_set_style_text_color(modal->hint, palette().red, 0);
      return;
    }
    close_modal(modal);
    platform().setExternalConfig(external.enabled, text, "");
    return;
  }

  if (text[0] == '\0') {
    if (external.token_set) {
      close_modal(modal);
      return;
    }
    lv_label_set_text(modal->hint, "Введите 32 hex-символа");
    lv_obj_set_style_text_color(modal->hint, palette().red, 0);
    return;
  }

  uint8_t token[kMiioTokenBytes] = {};
  if (!miio_parse_token(text, token)) {
    lv_label_set_text(modal->hint, "Нужно ровно 32 hex-символа");
    lv_obj_set_style_text_color(modal->hint, palette().red, 0);
    return;
  }

  close_modal(modal);
  platform().setExternalConfig(external.enabled, external.ip, text);
}

void modal_save_cb(lv_event_t* event) {
  save_modal(static_cast<TextEditModal*>(lv_event_get_user_data(event)));
}

void open_modal(ExternalScreen& screen, EditField field) {
  if (screen.modal != nullptr) return;

  const ExternalInfo external = g_state.snapshot().external;
  auto* modal = new TextEditModal();
  modal->screen = &screen;
  modal->field = field;
  screen.modal = modal;

  modal->overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(modal->overlay);
  lv_obj_set_pos(modal->overlay, 0, 0);
  lv_obj_set_size(modal->overlay, 480, 480);
  lv_obj_set_style_bg_color(modal->overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(modal->overlay, LV_OPA_60, 0);
  remove_flags(modal->overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(modal->overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* panel = make_card(modal->overlay, 10, 10, 460, 460);
  const bool is_ip = field == EditField::Ip;
  lv_obj_t* title = make_label(panel, is_ip ? "IP-адрес очистителя"
                                            : "Токен очистителя",
                               &theme().text28);
  lv_obj_set_pos(title, 18, 10);
  lv_obj_set_size(title, 424, 36);

  modal->hint = make_label(
      panel,
      is_ip ? "Например, 192.0.2.50"
            : (external.token_set ? "Пусто - оставить текущий токен"
                                  : "32 hex-символа; удобнее в /config"),
      &theme().muted14);
  lv_obj_set_pos(modal->hint, 18, 46);
  lv_obj_set_size(modal->hint, 424, 20);
  lv_label_set_long_mode(modal->hint, LV_LABEL_LONG_MODE_DOTS);

  modal->textarea = lv_textarea_create(panel);
  lv_obj_set_pos(modal->textarea, 18, 74);
  lv_obj_set_size(modal->textarea, 424, 48);
  lv_textarea_set_one_line(modal->textarea, true);
  lv_textarea_set_max_length(modal->textarea, is_ip ? 15 : 32);
  lv_textarea_set_accepted_chars(modal->textarea,
                                 is_ip ? "0123456789."
                                       : "0123456789abcdefABCDEF");
  lv_textarea_set_placeholder_text(
      modal->textarea, is_ip ? "192.0.2.50" : "32 hex-символа");
  if (!is_ip) {
    lv_textarea_set_password_mode(modal->textarea, true);
  }
  lv_obj_add_style(modal->textarea, &theme().text18, 0);
  if (is_ip && external.ip[0] != '\0') {
    lv_textarea_set_text(modal->textarea, external.ip);
  }

  lv_obj_t* keyboard = lv_keyboard_create(panel);
  lv_obj_set_align(keyboard, LV_ALIGN_DEFAULT);
  lv_obj_set_pos(keyboard, 10, 132);
  lv_obj_set_size(keyboard, 440, 260);
  lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_14, 0);
  lv_keyboard_set_textarea(keyboard, modal->textarea);

  lv_obj_t* cancel = lv_button_create(panel);
  lv_obj_set_pos(cancel, 18, 404);
  lv_obj_set_size(cancel, 200, 42);
  make_ghost_button(cancel);
  lv_obj_add_event_cb(cancel, modal_cancel_cb, LV_EVENT_CLICKED, modal);
  lv_obj_t* cancel_label = make_label(cancel, "Отмена", &theme().text18);
  lv_obj_set_size(cancel_label, 190, 24);
  lv_obj_set_style_text_align(cancel_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(cancel_label);

  lv_obj_t* save = lv_button_create(panel);
  lv_obj_set_pos(save, 242, 404);
  lv_obj_set_size(save, 200, 42);
  make_primary_button(save);
  lv_obj_add_event_cb(save, modal_save_cb, LV_EVENT_CLICKED, modal);
  lv_obj_t* save_label = make_label(save, "Сохранить", nullptr);
  lv_obj_set_style_text_font(save_label, &ui_font_18, 0);
  lv_obj_set_style_text_color(save_label, palette().dark_text, 0);
  lv_obj_set_size(save_label, 190, 24);
  lv_obj_set_style_text_align(save_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(save_label);
}

void ip_clicked_cb(lv_event_t* event) {
  auto* screen = static_cast<ExternalScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) open_modal(*screen, EditField::Ip);
}

void token_clicked_cb(lv_event_t* event) {
  auto* screen = static_cast<ExternalScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) open_modal(*screen, EditField::Token);
}

void enable_changed_cb(lv_event_t* event) {
  auto* screen = static_cast<ExternalScreen*>(lv_event_get_user_data(event));
  if (screen == nullptr || screen->enable_switch == nullptr ||
      screen->syncing_switch) {
    return;
  }
  const bool enabled =
      lv_obj_has_state(screen->enable_switch, LV_STATE_CHECKED);
  const ExternalInfo external = g_state.snapshot().external;

  if (enabled && (external.ip[0] == '\0' || !external.token_set)) {
    screen->syncing_switch = true;
    lv_obj_remove_state(screen->enable_switch, LV_STATE_CHECKED);
    screen->syncing_switch = false;
    set_label_text(screen->footer, "Сначала задайте IP и токен");
    return;
  }

  platform().setExternalConfig(enabled, external.ip, "");
  set_label_text(screen->footer, enabled ? "Опрос включён" : "Опрос выключен");
}

void check_clicked_cb(lv_event_t* event) {
  auto* screen = static_cast<ExternalScreen*>(lv_event_get_user_data(event));
  if (screen == nullptr) return;
  const ExternalInfo external = g_state.snapshot().external;
  if (!external.enabled || !external.configured) {
    set_label_text(screen->footer, "Сначала задайте IP и токен");
    return;
  }
  set_label_text(screen->footer, "Опрос очистителя...");
  platform().requestExternalRefresh();
}

}  // namespace

void external_screen_create(ExternalScreen& s, lv_event_cb_t version_cb,
                            lv_event_cb_t menu_cb) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  remove_flags(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  s.screen = scr;

  status_bar_create(s.status, scr, "←", menu_cb, version_cb);

  settings_tabs_create(scr, 3);

  lv_obj_t* title = make_label(scr, "Внешние устройства", &theme().text28);
  lv_obj_set_pos(title, 14, 94);
  lv_obj_set_size(title, 452, 32);

  lv_obj_t* panel = make_card(scr, 12, 134, 456, 286);

  lv_obj_t* enable_caption = make_label(panel, "Очиститель Xiaomi",
                                        &theme().text18);
  lv_obj_set_pos(enable_caption, 20, 12);
  lv_obj_set_size(enable_caption, 300, 26);

  s.enable_switch = lv_switch_create(panel);
  lv_obj_set_pos(s.enable_switch, 390, 12);
  lv_obj_set_size(s.enable_switch, 46, 26);
  lv_obj_add_style(s.enable_switch, &theme().switch_track, LV_PART_MAIN);
  lv_obj_add_style(s.enable_switch, &theme().switch_indicator_off,
                   LV_PART_INDICATOR);
  lv_obj_add_style(
      s.enable_switch, &theme().switch_indicator_on,
      static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_INDICATOR) |
                                       static_cast<uint32_t>(LV_STATE_CHECKED)));
  lv_obj_add_style(s.enable_switch, &theme().switch_knob, LV_PART_KNOB);
  lv_obj_add_event_cb(s.enable_switch, enable_changed_cb,
                      LV_EVENT_VALUE_CHANGED, &s);

  lv_obj_t* hint = make_label(panel, "Опрос по локальной сети каждые 5 минут",
                              &theme().muted14);
  lv_obj_set_pos(hint, 20, 44);
  lv_obj_set_size(hint, 416, 20);

  make_divider(panel, 20, 72, 416);

  make_edit_row(panel, 82, "IP-адрес", ip_clicked_cb, &s, &s.ip_summary);
  make_edit_row(panel, 128, "Токен", token_clicked_cb, &s, &s.token_summary);

  make_divider(panel, 20, 178, 416);

  s.status_dot = make_dot(panel, 20, 190, 10, palette().muted);
  s.status_label = make_label(panel, "Не настроено", &theme().text14);
  lv_obj_set_pos(s.status_label, 38, 184);
  lv_obj_set_size(s.status_label, 398, 20);
  lv_label_set_long_mode(s.status_label, LV_LABEL_LONG_MODE_DOTS);

  s.model_label = make_label(panel, "Модель не определена", &theme().muted14);
  lv_obj_set_pos(s.model_label, 20, 204);
  lv_obj_set_size(s.model_label, 416, 20);
  lv_label_set_long_mode(s.model_label, LV_LABEL_LONG_MODE_DOTS);

  s.updated_label = make_label(panel, "Ещё не опрашивался", &theme().muted14);
  lv_obj_set_pos(s.updated_label, 20, 226);
  lv_obj_set_size(s.updated_label, 416, 20);
  lv_label_set_long_mode(s.updated_label, LV_LABEL_LONG_MODE_DOTS);

  s.check_button = lv_button_create(panel);
  lv_obj_set_pos(s.check_button, 20, 250);
  lv_obj_set_size(s.check_button, 200, 36);
  make_primary_button(s.check_button);
  lv_obj_add_event_cb(s.check_button, check_clicked_cb, LV_EVENT_CLICKED, &s);
  lv_obj_t* check_label = make_label(s.check_button, "Проверить сейчас",
                                      nullptr);
  lv_obj_set_style_text_font(check_label, &ui_font_14, 0);
  lv_obj_set_style_text_color(check_label, palette().dark_text, 0);
  lv_obj_set_size(check_label, 194, 24);
  lv_obj_set_style_text_align(check_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(check_label);

  lv_obj_t* check_hint = make_label(panel, "Опрос каждые 5 минут",
                                     &theme().muted14);
  lv_obj_set_pos(check_hint, 230, 258);
  lv_obj_set_size(check_hint, 206, 20);
  lv_obj_set_style_text_align(check_hint, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(check_hint, LV_LABEL_LONG_MODE_DOTS);

  s.footer = make_label(scr, "Настройки сохраняются автоматически",
                        &theme().muted14);
  lv_obj_set_pos(s.footer, 12, 430);
  lv_obj_set_size(s.footer, 456, 20);
  lv_obj_set_style_text_align(s.footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(s.footer, LV_LABEL_LONG_MODE_DOTS);
}

void external_screen_update(ExternalScreen& s, const AppState& state) {
  status_bar_update(s.status, state);
  const ExternalInfo& external = state.external;

  s.syncing_switch = true;
  if (external.enabled) {
    lv_obj_add_state(s.enable_switch, LV_STATE_CHECKED);
  } else {
    lv_obj_remove_state(s.enable_switch, LV_STATE_CHECKED);
  }
  s.syncing_switch = false;

  set_label_text_safe(s.ip_summary,
                      external.ip[0] != '\0' ? external.ip : "Не задан");
  set_label_text(s.token_summary,
                 external.token_set ? "Задан" : "Не задан");

  lv_color_t dot = palette().muted;
  if (!external.enabled) {
    dot = palette().muted;
  } else if (!external.configured) {
    dot = palette().red;
  } else if (external.online) {
    dot = palette().green;
  } else if (external.valid) {
    dot = palette().amber;
  } else {
    dot = palette().red;
  }
  lv_obj_set_style_bg_color(s.status_dot, dot, 0);

  if (external.status[0] != '\0') {
    set_label_text_safe(s.status_label, external.status);
  } else if (!external.enabled) {
    set_label_text(s.status_label, "Выключено");
  } else if (!external.configured) {
    set_label_text(s.status_label, "Не настроено");
  } else if (external.online) {
    set_label_text(s.status_label, "Подключено");
  } else {
    set_label_text(s.status_label, "Нет связи");
  }

  set_label_text_safe(s.model_label,
                      external.model[0] != '\0' ? external.model
                                                : "Модель не определена");

  char buffer[80];
  if (external.valid) {
    uint32_t age = 0;
    if (state.uptime_s >= external.updated_uptime_s) {
      age = state.uptime_s - external.updated_uptime_s;
    }
    if (age < 60) {
      set_label_text(s.updated_label, "Обновлено только что");
    } else {
      std::snprintf(buffer, sizeof(buffer), "Обновлено %u мин назад",
                    static_cast<unsigned>(age / 60U));
      set_label_text(s.updated_label, buffer);
    }
  } else {
    set_label_text(s.updated_label, "Ещё не опрашивался");
  }

  if (external.enabled && external.configured) {
    lv_obj_remove_state(s.check_button, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(s.check_button, LV_STATE_DISABLED);
  }
}

void external_hide_modal(ExternalScreen& screen) {
  close_modal(static_cast<TextEditModal*>(screen.modal));
}

bool external_modal_visible(const ExternalScreen& screen) {
  return screen.modal != nullptr;
}

void external_modal_set_text(ExternalScreen& screen, const char* text) {
  auto* modal = static_cast<TextEditModal*>(screen.modal);
  if (modal != nullptr && modal->textarea != nullptr) {
    lv_textarea_set_text(modal->textarea, text != nullptr ? text : "");
  }
}

void external_modal_save(ExternalScreen& screen) {
  save_modal(static_cast<TextEditModal*>(screen.modal));
}

void external_edit_ip(ExternalScreen& screen) {
  open_modal(screen, EditField::Ip);
}

void external_edit_token(ExternalScreen& screen) {
  open_modal(screen, EditField::Token);
}

}  // namespace ui
}  // namespace tablo
