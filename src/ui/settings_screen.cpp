#include "settings_screen.h"

#include <cstdio>

#include "core/platform.h"
#include "fonts/fonts.h"
#include "settings_tabs.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

void brightness_changed_cb(lv_event_t* event) {
  auto* settings = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
  if (settings == nullptr || settings->slider == nullptr ||
      settings->syncing_slider) {
    return;
  }
  int value = lv_slider_get_value(settings->slider);
  if (value < 10) {
    value = 10;
  } else if (value > 100) {
    value = 100;
  }
  char buffer[12];
  std::snprintf(buffer, sizeof(buffer), "%d%%", value);
  set_label_text(settings->brightness_value, buffer);

  settings->brightness_override = true;
  settings->brightness_desired = static_cast<uint8_t>(value);
  settings->brightness_until = lv_tick_get();
  platform().setBrightness(static_cast<uint8_t>(value));
}

void night_changed_cb(lv_event_t* event) {
  auto* settings = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
  if (settings == nullptr || settings->night_switch == nullptr ||
      settings->syncing_switch) {
    return;
  }
  const bool enabled =
      lv_obj_has_state(settings->night_switch, LV_STATE_CHECKED);
  settings->night_override = true;
  settings->night_desired = enabled;
  settings->night_until = lv_tick_get();
  platform().setNightMode(enabled);
}

}  // namespace

lv_obj_t* make_nav_row(lv_obj_t* parent, int32_t y, const char* caption,
                       lv_event_cb_t callback, SettingsScreen* s,
                       lv_obj_t** summary) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_add_style(button, &theme().button_ghost, 0);
  lv_obj_add_style(button, &theme().button_ghost_pressed, LV_STATE_PRESSED);
  lv_obj_set_pos(button, 20, y);
  lv_obj_set_size(button, 416, 42);
  lv_obj_set_style_shadow_width(button, 0, 0);
  remove_flags(button, LV_OBJ_FLAG_SCROLLABLE);
  if (callback != nullptr) {
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, s);
  }

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

void settings_create(SettingsScreen& s, lv_event_cb_t version_cb,
                     lv_event_cb_t gesture_cb, lv_event_cb_t menu_cb,
                     lv_event_cb_t schedule_cb) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  remove_flags(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  if (gesture_cb != nullptr) {
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, nullptr);
  }
  s.screen = scr;

  status_bar_create(s.status, scr, "←", menu_cb, version_cb);

  settings_tabs_create(scr, 0);

  lv_obj_t* panel = make_card(scr, 12, 100, 456, 300);

  lv_obj_t* brightness_caption = make_label(panel, "Яркость",
                                             &theme().text18);
  lv_obj_set_pos(brightness_caption, 20, 18);
  lv_obj_set_size(brightness_caption, 220, 26);

  s.brightness_value = make_label(panel, "80%", &theme().accent18);
  lv_obj_set_pos(s.brightness_value, 280, 18);
  lv_obj_set_size(s.brightness_value, 156, 26);
  lv_obj_set_style_text_align(s.brightness_value, LV_TEXT_ALIGN_RIGHT, 0);

  s.slider = lv_slider_create(panel);
  lv_obj_set_pos(s.slider, 20, 58);
  lv_obj_set_size(s.slider, 416, 8);
  lv_slider_set_range(s.slider, 10, 100);
  lv_slider_set_value(s.slider, 80, LV_ANIM_OFF);
  lv_obj_add_style(s.slider, &theme().slider_track, LV_PART_MAIN);
  lv_obj_add_style(s.slider, &theme().slider_indicator, LV_PART_INDICATOR);
  lv_obj_add_style(s.slider, &theme().slider_knob, LV_PART_KNOB);
  lv_obj_add_event_cb(s.slider, brightness_changed_cb,
                      LV_EVENT_VALUE_CHANGED, &s);

  make_divider(panel, 20, 92, 416);

  lv_obj_t* night_caption = make_label(panel, "Ночной режим",
                                       &theme().text18);
  lv_obj_set_pos(night_caption, 20, 114);
  lv_obj_set_size(night_caption, 260, 26);

  s.night_switch = lv_switch_create(panel);
  lv_obj_set_pos(s.night_switch, 390, 112);
  lv_obj_set_size(s.night_switch, 46, 26);
  lv_obj_add_style(s.night_switch, &theme().switch_track, LV_PART_MAIN);
  lv_obj_add_style(s.night_switch, &theme().switch_indicator_off,
                   LV_PART_INDICATOR);
  lv_obj_add_style(
      s.night_switch, &theme().switch_indicator_on,
      static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_INDICATOR) |
                                       static_cast<uint32_t>(LV_STATE_CHECKED)));
  lv_obj_add_style(s.night_switch, &theme().switch_knob, LV_PART_KNOB);
  lv_obj_set_style_opa(s.night_switch, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_add_event_cb(s.night_switch, night_changed_cb,
                      LV_EVENT_VALUE_CHANGED, &s);

  s.schedule_button = make_nav_row(panel, 160, "Расписание", schedule_cb, &s,
                                   &s.schedule_summary);

  lv_obj_t* hint = make_label(panel, "Настройки сохраняются автоматически",
                              &theme().muted14);
  lv_obj_set_pos(hint, 20, 260);
  lv_obj_set_size(hint, 416, 18);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

  s.footer = make_label(scr, "", &theme().muted14);
  lv_obj_set_pos(s.footer, 12, 430);
  lv_obj_set_size(s.footer, 456, 20);
  lv_obj_set_style_text_align(s.footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(s.footer, LV_LABEL_LONG_MODE_DOTS);
}

void settings_update(SettingsScreen& s, const AppState& state) {
  status_bar_update(s.status, state);

  char buffer[96];
  char evening[8];
  char night[8];
  char day[8];
  format_hhmm(state.schedule.evening_min, evening, sizeof(evening));
  format_hhmm(state.schedule.night_min, night, sizeof(night));
  format_hhmm(state.schedule.day_min, day, sizeof(day));
  std::snprintf(buffer, sizeof(buffer), "%s / %s / %s%s", evening, night, day,
                state.schedule.enabled ? "  авто" : "");
  set_label_text_safe(s.schedule_summary, buffer);

  /* Never pull the slider back while the user is holding it. */
  int desired_brightness = state.brightness;
  if (desired_brightness < 10) {
    desired_brightness = 10;
  } else if (desired_brightness > 100) {
    desired_brightness = 100;
  }

  if (s.brightness_override) {
    if (desired_brightness == s.brightness_desired ||
        lv_tick_elaps(s.brightness_until) > 3000U) {
      s.brightness_override = false;
    }
  }

  if (!s.brightness_override) {
    if (!lv_slider_is_dragged(s.slider) &&
        lv_slider_get_value(s.slider) != desired_brightness) {
      s.syncing_slider = true;
      lv_slider_set_value(s.slider, desired_brightness, LV_ANIM_OFF);
      s.syncing_slider = false;
    }
    if (!lv_slider_is_dragged(s.slider)) {
      std::snprintf(buffer, sizeof(buffer), "%d%%", desired_brightness);
      set_label_text(s.brightness_value, buffer);
    }
  }

  /* Same protection for the switch; the state round-trip may lag. */
  if (s.night_override) {
    if (state.night_mode == s.night_desired ||
        lv_tick_elaps(s.night_until) > 3000U) {
      s.night_override = false;
    }
  }

  if (state.schedule.enabled) {
    lv_obj_add_state(s.night_switch, LV_STATE_DISABLED);
  } else {
    lv_obj_remove_state(s.night_switch, LV_STATE_DISABLED);
  }

  if (!s.night_override && !state.schedule.enabled &&
      !lv_obj_has_state(s.night_switch, LV_STATE_PRESSED)) {
    const bool checked = lv_obj_has_state(s.night_switch, LV_STATE_CHECKED);
    if (checked != state.night_mode) {
      s.syncing_switch = true;
      if (state.night_mode) {
        lv_obj_add_state(s.night_switch, LV_STATE_CHECKED);
      } else {
        lv_obj_remove_state(s.night_switch, LV_STATE_CHECKED);
      }
      s.syncing_switch = false;
    }
  } else if (state.schedule.enabled) {
    const bool should_check = state.display_mode == DisplayMode::Night;
    const bool checked = lv_obj_has_state(s.night_switch, LV_STATE_CHECKED);
    if (checked != should_check) {
      s.syncing_switch = true;
      if (should_check) {
        lv_obj_add_state(s.night_switch, LV_STATE_CHECKED);
      } else {
        lv_obj_remove_state(s.night_switch, LV_STATE_CHECKED);
      }
      s.syncing_switch = false;
    }
  }

  if (state.ota.downloading) {
    std::snprintf(buffer, sizeof(buffer), "Загрузка обновления: %d%%",
                  static_cast<int>(state.ota.progress));
  } else if (state.ota.available && state.ota.version[0] != '\0') {
    std::snprintf(buffer, sizeof(buffer), "Доступно обновление %s",
                  state.ota.version);
  } else if (state.ota.status[0] != '\0') {
    std::snprintf(buffer, sizeof(buffer), "%s", state.ota.status);
  } else {
    char uptime[48];
    format_uptime(state.uptime_s, uptime, sizeof(uptime));
    std::snprintf(buffer, sizeof(buffer), "Время работы: %s", uptime);
  }
  set_label_text_safe(s.footer, buffer);
}

}  // namespace ui
}  // namespace tablo
