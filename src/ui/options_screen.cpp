#include "options_screen.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/date_format.h"
#include "core/platform.h"
#include "fonts/fonts.h"
#include "settings_tabs.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

constexpr int kTzMin = -12;
constexpr int kTzMax = 14;
constexpr int kTzCount = kTzMax - kTzMin + 1;

const char* utc_label(int hours) {
  static char labels[kTzCount][10];
  static bool ready = false;
  if (!ready) {
    for (int h = kTzMin; h <= kTzMax; ++h) {
      if (h == 0) {
        std::snprintf(labels[h - kTzMin], sizeof(labels[0]), "UTC");
      } else {
        std::snprintf(labels[h - kTzMin], sizeof(labels[0]), "UTC%+d", h);
      }
    }
    ready = true;
  }
  const int index = hours < kTzMin ? 0 : (hours > kTzMax ? kTzCount - 1
                                                          : hours - kTzMin);
  return labels[index];
}

bool parse_utc_offset(const char* timezone, int& hours) {
  if (timezone == nullptr || timezone[0] == '\0') return false;
  const char* plus = std::strrchr(timezone, '+');
  const char* minus = std::strrchr(timezone, '-');
  const char* sign = plus;
  if (minus != nullptr && (sign == nullptr || minus > sign)) sign = minus;
  if (sign == nullptr) {
    // "UTC0" and similar names without an explicit offset.
    const char* digits = timezone;
    while (*digits != '\0' && (*digits < '0' || *digits > '9')) ++digits;
    if (*digits == '\0') return false;
    hours = std::atoi(digits);
  } else {
    const int value = std::atoi(sign + 1);
    hours = *sign == '-' ? value : -value;
  }
  if (hours < kTzMin) hours = kTzMin;
  if (hours > kTzMax) hours = kTzMax;
  return true;
}

void make_posix_tz(int hours, char* out, size_t cap) {
  if (hours == 0) {
    std::snprintf(out, cap, "UTC0");
  } else if (hours > 0) {
    std::snprintf(out, cap, "UTC-%d", hours);
  } else {
    std::snprintf(out, cap, "UTC+%d", -hours);
  }
}

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

void style_roller(lv_obj_t* roller) {
  lv_obj_add_style(roller, &theme().text28, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);
  lv_obj_add_style(roller, &theme().text28, LV_PART_SELECTED);
  lv_obj_add_style(roller, &theme().button_primary, LV_PART_SELECTED);
  lv_obj_set_style_pad_ver(roller, 6, LV_PART_SELECTED);
}

struct ChoiceModal {
  OptionsScreen* screen = nullptr;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* roller = nullptr;
  bool timezone = false;
};

void close_choice(ChoiceModal* modal) {
  if (modal == nullptr) return;
  if (modal->screen != nullptr) modal->screen->choice = nullptr;
  if (modal->overlay != nullptr) lv_obj_delete(modal->overlay);
  delete modal;
}

void choice_cancel_cb(lv_event_t* event) {
  close_choice(static_cast<ChoiceModal*>(lv_event_get_user_data(event)));
}

void choice_save_cb(lv_event_t* event) {
  auto* modal = static_cast<ChoiceModal*>(lv_event_get_user_data(event));
  if (modal == nullptr || modal->roller == nullptr) return;
  const int index = lv_roller_get_selected(modal->roller);
  if (modal->timezone) {
    char timezone[16] = {};
    make_posix_tz(index + kTzMin, timezone, sizeof(timezone));
    platform().setTimezone(timezone);
  } else {
    platform().setDateFormat(static_cast<DateFormat>(index));
  }
  close_choice(modal);
}

void open_choice(OptionsScreen& screen, bool timezone) {
  if (screen.choice != nullptr) return;
  auto* modal = new ChoiceModal();
  modal->screen = &screen;
  modal->timezone = timezone;
  screen.choice = modal;

  modal->overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(modal->overlay);
  lv_obj_set_pos(modal->overlay, 0, 0);
  lv_obj_set_size(modal->overlay, 480, 480);
  lv_obj_set_style_bg_color(modal->overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(modal->overlay, LV_OPA_50, 0);
  remove_flags(modal->overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(modal->overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* panel = make_card(modal->overlay, 50, 70, 380, 340);
  lv_obj_t* title = make_label(panel, timezone ? "Часовой пояс" : "Формат даты",
                                &theme().text28);
  lv_obj_set_pos(title, 20, 12);
  lv_obj_set_size(title, 340, 40);

  char options[512] = {};
  size_t used = 0;
  if (timezone) {
    for (int h = kTzMin; h <= kTzMax; ++h) {
      const int written = std::snprintf(options + used, sizeof(options) - used,
                                        h == kTzMin ? "%s" : "\n%s", utc_label(h));
      if (written < 0 || static_cast<size_t>(written) >= sizeof(options) - used) {
        options[used] = '\0';
        break;
      }
      used += static_cast<size_t>(written);
    }
  } else {
    for (int i = 0; i < 3; ++i) {
      const int written = std::snprintf(
          options + used, sizeof(options) - used, i == 0 ? "%s" : "\n%s",
          date_format_label(static_cast<DateFormat>(i)));
      if (written < 0 || static_cast<size_t>(written) >= sizeof(options) - used) {
        options[used] = '\0';
        break;
      }
      used += static_cast<size_t>(written);
    }
  }

  modal->roller = lv_roller_create(panel);
  lv_obj_set_pos(modal->roller, 30, 62);
  lv_obj_set_size(modal->roller, 320, 150);
  lv_roller_set_options(modal->roller, options, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(modal->roller, 3);
  const int selected = timezone ? screen.timezone_hours - kTzMin
                                : static_cast<int>(screen.date_format);
  lv_roller_set_selected(modal->roller, static_cast<uint16_t>(selected),
                         LV_ANIM_OFF);
  style_roller(modal->roller);

  lv_obj_t* cancel = lv_button_create(panel);
  lv_obj_set_pos(cancel, 30, 236);
  lv_obj_set_size(cancel, 150, 44);
  make_ghost_button(cancel);
  lv_obj_add_event_cb(cancel, choice_cancel_cb, LV_EVENT_CLICKED, modal);
  lv_obj_t* cancel_label = make_label(cancel, "Отмена", &theme().text18);
  lv_obj_set_size(cancel_label, 144, 24);
  lv_obj_set_style_text_align(cancel_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(cancel_label);

  lv_obj_t* save = lv_button_create(panel);
  lv_obj_set_pos(save, 200, 236);
  lv_obj_set_size(save, 150, 44);
  make_primary_button(save);
  lv_obj_add_event_cb(save, choice_save_cb, LV_EVENT_CLICKED, modal);
  lv_obj_t* save_label = make_label(save, "Готово", nullptr);
  lv_obj_set_style_text_font(save_label, &ui_font_18, 0);
  lv_obj_set_style_text_color(save_label, palette().dark_text, 0);
  lv_obj_set_size(save_label, 144, 24);
  lv_obj_set_style_text_align(save_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(save_label);
}

void timezone_cb(lv_event_t* event) {
  auto* screen = static_cast<OptionsScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) open_choice(*screen, true);
}

void date_format_cb(lv_event_t* event) {
  auto* screen = static_cast<OptionsScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) open_choice(*screen, false);
}

void weather_cb(lv_event_t* event) {
  auto* screen = static_cast<OptionsScreen*>(lv_event_get_user_data(event));
  if (screen == nullptr || screen->weather_switch == nullptr) return;
  platform().setWeatherEnabled(
      lv_obj_has_state(screen->weather_switch, LV_STATE_CHECKED));
}

lv_obj_t* make_row(lv_obj_t* parent, int y, const char* caption,
                   lv_event_cb_t callback, OptionsScreen* screen,
                   lv_obj_t** value_out) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_add_style(button, &theme().button_ghost, 0);
  lv_obj_add_style(button, &theme().button_ghost_pressed, LV_STATE_PRESSED);
  lv_obj_set_pos(button, 16, y);
  lv_obj_set_size(button, 424, 40);
  lv_obj_set_style_shadow_width(button, 0, 0);
  remove_flags(button, LV_OBJ_FLAG_SCROLLABLE);
  if (callback != nullptr) {
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, screen);
  }
  lv_obj_t* label = make_label(button, caption, &theme().text18);
  lv_obj_set_pos(label, 10, 7);
  lv_obj_set_size(label, 180, 26);
  *value_out = make_label(button, "", &theme().muted14);
  lv_obj_set_pos(*value_out, 190, 10);
  lv_obj_set_size(*value_out, 200, 22);
  lv_obj_set_style_text_align(*value_out, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(*value_out, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_t* arrow = make_label(button, ">", &theme().accent14);
  lv_obj_set_pos(arrow, 396, 9);
  lv_obj_set_size(arrow, 20, 22);
  lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_RIGHT, 0);
  return button;
}

}  // namespace

void options_create(OptionsScreen& s, lv_event_cb_t version_cb,
                    lv_event_cb_t menu_cb, lv_event_cb_t wifi_cb) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  remove_flags(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  s.screen = scr;

  status_bar_create(s.status, scr, "←", menu_cb, version_cb);

  settings_tabs_create(scr, 1);

  lv_obj_t* title = make_label(scr, "Локальные настройки", &theme().text28);
  lv_obj_set_pos(title, 14, 94);
  lv_obj_set_size(title, 452, 32);

  lv_obj_t* panel = make_card(scr, 12, 134, 456, 286);

  make_row(panel, 14, "Часовой пояс", timezone_cb, &s, &s.timezone_value);
  lv_obj_t* date_row = make_row(panel, 110, "Формат даты", date_format_cb, &s,
                                &s.date_format_value);
  (void)date_row;

  lv_obj_t* weather_row = lv_obj_create(panel);
  lv_obj_remove_style_all(weather_row);
  lv_obj_add_style(weather_row, &theme().button_ghost, 0);
  lv_obj_set_pos(weather_row, 16, 62);
  lv_obj_set_size(weather_row, 424, 40);
  lv_obj_set_style_shadow_width(weather_row, 0, 0);
  remove_flags(weather_row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* weather_caption = make_label(panel, "Погода", &theme().text18);
  lv_obj_set_pos(weather_caption, 26, 69);
  lv_obj_set_size(weather_caption, 220, 26);
  s.weather_switch = lv_switch_create(panel);
  lv_obj_set_pos(s.weather_switch, 394, 67);
  lv_obj_set_size(s.weather_switch, 46, 26);
  lv_obj_add_style(s.weather_switch, &theme().switch_track, LV_PART_MAIN);
  lv_obj_add_style(s.weather_switch, &theme().switch_indicator_off,
                   LV_PART_INDICATOR);
  lv_obj_add_style(
      s.weather_switch, &theme().switch_indicator_on,
      static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_INDICATOR) |
                                       static_cast<uint32_t>(LV_STATE_CHECKED)));
  lv_obj_add_style(s.weather_switch, &theme().switch_knob, LV_PART_KNOB);
  lv_obj_add_event_cb(s.weather_switch, weather_cb, LV_EVENT_VALUE_CHANGED, &s);

  lv_obj_t* wifi_row = make_row(panel, 158, "Wi-Fi", wifi_cb, &s, &s.wifi_value);
  (void)wifi_row;

  make_divider(panel, 16, 208, 424);
  lv_obj_t* hint = make_label(
      panel,
      "Часовой пояс влияет на расписание и дату. Wi-Fi меняется без веб-интерфейса.",
      &theme().muted14);
  lv_obj_set_pos(hint, 16, 222);
  lv_obj_set_size(hint, 424, 44);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);

  s.footer = make_label(scr, "Изменения сохраняются автоматически", &theme().muted14);
  lv_obj_set_pos(s.footer, 12, 430);
  lv_obj_set_size(s.footer, 456, 20);
  lv_obj_set_style_text_align(s.footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(s.footer, LV_LABEL_LONG_MODE_DOTS);
}

void options_update(OptionsScreen& s, const AppState& state) {
  status_bar_update(s.status, state);

  int hours = 3;
  if (!parse_utc_offset(state.timezone, hours)) hours = 3;
  s.timezone_hours = hours;
  set_label_text(s.timezone_value, utc_label(hours));

  s.date_format = state.date_format;
  set_label_text_safe(s.date_format_value,
                      date_format_label(state.date_format));

  if (state.weather_enabled != s.last_weather_enabled) {
    s.last_weather_enabled = state.weather_enabled;
    if (state.weather_enabled) {
      lv_obj_add_state(s.weather_switch, LV_STATE_CHECKED);
    } else {
      lv_obj_remove_state(s.weather_switch, LV_STATE_CHECKED);
    }
  }

  set_label_text_safe(s.wifi_value,
                      state.wifi_ssid[0] != '\0' ? state.wifi_ssid
                                                  : "Не задана");
}

}  // namespace ui
}  // namespace tablo
