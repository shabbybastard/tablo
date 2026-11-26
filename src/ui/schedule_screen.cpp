#include "schedule_screen.h"

#include <cstdio>

#include "core/platform.h"
#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

constexpr uint32_t kOverrideMs = 3000U;

bool same_times(const NightSchedule& a, const NightSchedule& b) {
  return a.evening_min == b.evening_min && a.night_min == b.night_min &&
         a.day_min == b.day_min;
}

const char* field_title(ScheduleField field) {
  switch (field) {
    case ScheduleField::Evening:
      return "Вечер";
    case ScheduleField::Night:
      return "Ночь";
    case ScheduleField::Day:
      return "День";
  }
  return "Время";
}

uint16_t field_minutes(const NightSchedule& schedule, ScheduleField field) {
  switch (field) {
    case ScheduleField::Evening:
      return schedule.evening_min;
    case ScheduleField::Night:
      return schedule.night_min;
    case ScheduleField::Day:
      return schedule.day_min;
  }
  return 0;
}

void set_field_minutes(NightSchedule& schedule, ScheduleField field,
                       uint16_t minutes) {
  switch (field) {
    case ScheduleField::Evening:
      schedule.evening_min = minutes;
      break;
    case ScheduleField::Night:
      schedule.night_min = minutes;
      break;
    case ScheduleField::Day:
      schedule.day_min = minutes;
      break;
  }
}

lv_obj_t* field_label(const ScheduleScreen& screen, ScheduleField field) {
  switch (field) {
    case ScheduleField::Evening:
      return screen.evening_value;
    case ScheduleField::Night:
      return screen.night_value;
    case ScheduleField::Day:
      return screen.day_value;
  }
  return nullptr;
}

const char* mode_title(DisplayMode mode) {
  switch (mode) {
    case DisplayMode::Day:
      return "день";
    case DisplayMode::Evening:
      return "вечер";
    case DisplayMode::Night:
      return "ночь";
  }
  return "день";
}

void set_hidden(lv_obj_t* object, bool hidden) {
  if (object == nullptr) {
    return;
  }
  if (hidden) {
    lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
  } else {
    remove_flags(object, LV_OBJ_FLAG_HIDDEN);
  }
}

const char* apply_time_cb(void* user_data, uint16_t minutes);

void open_picker(ScheduleScreen& screen, ScheduleField field) {
  screen.editing = field;
  time_picker_show(screen.picker, field_title(field),
                   field_minutes(screen.schedule, field), apply_time_cb,
                   &screen);
}

const char* apply_time_cb(void* user_data, uint16_t minutes) {
  auto* screen = static_cast<ScheduleScreen*>(user_data);
  if (screen == nullptr) {
    return "Не удалось сохранить время.";
  }

  NightSchedule updated = screen->schedule;
  set_field_minutes(updated, screen->editing, minutes);
  if (!night_schedule_valid(updated)) {
    return "Порядок: день < вечер < ночь.";
  }

  screen->schedule = updated;
  screen->values_override = true;
  screen->values_desired = updated;
  screen->values_until = lv_tick_get();

  char text[8];
  format_hhmm(minutes, text, sizeof(text));
  set_label_text(field_label(*screen, screen->editing), text);
  set_hidden(screen->warning, true);

  platform().setNightSchedule(updated);
  return nullptr;
}

void evening_clicked_cb(lv_event_t* event) {
  auto* screen = static_cast<ScheduleScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) {
    open_picker(*screen, ScheduleField::Evening);
  }
}

void night_clicked_cb(lv_event_t* event) {
  auto* screen = static_cast<ScheduleScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) {
    open_picker(*screen, ScheduleField::Night);
  }
}

void day_clicked_cb(lv_event_t* event) {
  auto* screen = static_cast<ScheduleScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) {
    open_picker(*screen, ScheduleField::Day);
  }
}

void auto_changed_cb(lv_event_t* event) {
  auto* screen = static_cast<ScheduleScreen*>(lv_event_get_user_data(event));
  if (screen == nullptr || screen->auto_switch == nullptr ||
      screen->syncing_switch) {
    return;
  }

  const bool enabled =
      lv_obj_has_state(screen->auto_switch, LV_STATE_CHECKED);
  NightSchedule updated = screen->schedule;
  updated.enabled = enabled;

  screen->schedule = updated;
  screen->auto_override = true;
  screen->auto_desired = enabled;
  screen->auto_until = lv_tick_get();
  platform().setNightSchedule(updated);
}

lv_obj_t* make_time_row(lv_obj_t* parent, int32_t y, const char* title,
                        lv_event_cb_t callback, ScheduleScreen& screen,
                        lv_obj_t** value_out) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_pos(row, 8, y);
  lv_obj_set_size(row, 440, 54);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  remove_flags(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_add_style(row, &theme().button_ghost_pressed, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(row, 0, LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, callback, LV_EVENT_CLICKED, &screen);

  lv_obj_t* caption = make_label(row, title, &theme().text18);
  lv_obj_set_size(caption, 180, 26);
  lv_obj_align(caption, LV_ALIGN_LEFT_MID, 12, 0);

  lv_obj_t* value = make_label(row, "00:00", &theme().accent18);
  lv_obj_set_size(value, 140, 26);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(value, LV_ALIGN_RIGHT_MID, -42, 0);

  lv_obj_t* chevron = make_label(row, ">", &theme().accent18);
  lv_obj_set_style_text_font(chevron, &ui_font_28, 0);
  lv_obj_set_size(chevron, 20, 32);
  lv_obj_set_style_text_align(chevron, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -12, 0);

  if (value_out != nullptr) {
    *value_out = value;
  }
  return row;
}

}  // namespace

void schedule_screen_create(ScheduleScreen& s, lv_event_cb_t back_cb) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  remove_flags(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  s.screen = scr;

  lv_obj_t* title = make_label(scr, "Расписание", &theme().text28);
  lv_obj_set_pos(title, 14, 12);
  lv_obj_set_size(title, 320, 38);
  lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);

  s.back_button = lv_button_create(scr);
  lv_obj_remove_style_all(s.back_button);
  lv_obj_add_style(s.back_button, &theme().button_ghost, 0);
  lv_obj_add_style(s.back_button, &theme().button_ghost_pressed,
                   LV_STATE_PRESSED);
  lv_obj_set_pos(s.back_button, 352, 12);
  lv_obj_set_size(s.back_button, 114, 34);
  lv_obj_set_style_shadow_width(s.back_button, 0, 0);
  remove_flags(s.back_button, LV_OBJ_FLAG_SCROLLABLE);
  if (back_cb != nullptr) {
    lv_obj_add_event_cb(s.back_button, back_cb, LV_EVENT_CLICKED, nullptr);
  }

  lv_obj_t* back_label = make_label(s.back_button, "Назад",
                                    &theme().muted14);
  lv_obj_set_size(back_label, 108, 20);
  lv_obj_set_style_text_align(back_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(back_label);

  lv_obj_t* subtitle = make_label(scr, "Автоматическое переключение режимов",
                                  &theme().muted14);
  lv_obj_set_pos(subtitle, 14, 54);
  lv_obj_set_size(subtitle, 452, 20);
  lv_label_set_long_mode(subtitle, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* panel = make_card(scr, 12, 82, 456, 326);

  lv_obj_t* auto_caption = make_label(panel, "Авто по времени",
                                      &theme().text18);
  lv_obj_set_pos(auto_caption, 16, 14);
  lv_obj_set_size(auto_caption, 280, 26);

  s.auto_switch = lv_switch_create(panel);
  lv_obj_set_pos(s.auto_switch, 394, 16);
  lv_obj_set_size(s.auto_switch, 46, 26);
  lv_obj_add_style(s.auto_switch, &theme().switch_track, LV_PART_MAIN);
  lv_obj_add_style(s.auto_switch, &theme().switch_indicator_off,
                   LV_PART_INDICATOR);
  lv_obj_add_style(
      s.auto_switch, &theme().switch_indicator_on,
      static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_INDICATOR) |
                                       static_cast<uint32_t>(LV_STATE_CHECKED)));
  lv_obj_add_style(s.auto_switch, &theme().switch_knob, LV_PART_KNOB);
  lv_obj_add_event_cb(s.auto_switch, auto_changed_cb, LV_EVENT_VALUE_CHANGED,
                      &s);
  if (s.schedule.enabled) {
    s.syncing_switch = true;
    lv_obj_add_state(s.auto_switch, LV_STATE_CHECKED);
    s.syncing_switch = false;
  }

  lv_obj_t* auto_hint = make_label(panel, "Вечер, ночь и день по часам",
                                   &theme().muted14);
  lv_obj_set_pos(auto_hint, 16, 42);
  lv_obj_set_size(auto_hint, 360, 18);
  lv_label_set_long_mode(auto_hint, LV_LABEL_LONG_MODE_DOTS);

  make_divider(panel, 16, 68, 424);

  s.evening_row = make_time_row(panel, 70, "Вечер", evening_clicked_cb, s,
                                &s.evening_value);
  make_divider(panel, 16, 124, 424);
  s.night_row = make_time_row(panel, 126, "Ночь", night_clicked_cb, s,
                              &s.night_value);
  make_divider(panel, 16, 180, 424);
  s.day_row = make_time_row(panel, 182, "День", day_clicked_cb, s,
                            &s.day_value);
  make_divider(panel, 16, 236, 424);

  char initial[8];
  format_hhmm(s.schedule.evening_min, initial, sizeof(initial));
  set_label_text(s.evening_value, initial);
  format_hhmm(s.schedule.night_min, initial, sizeof(initial));
  set_label_text(s.night_value, initial);
  format_hhmm(s.schedule.day_min, initial, sizeof(initial));
  set_label_text(s.day_value, initial);

  s.status = make_label(panel, "Сейчас: день", &theme().text14);
  lv_obj_set_pos(s.status, 16, 246);
  lv_obj_set_size(s.status, 424, 20);

  s.warning = make_label(panel, "", &theme().text14);
  lv_obj_set_pos(s.warning, 16, 270);
  lv_obj_set_size(s.warning, 424, 38);
  lv_obj_set_style_text_color(s.warning, palette().amber, 0);
  lv_label_set_long_mode(s.warning, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_add_flag(s.warning, LV_OBJ_FLAG_HIDDEN);
  s.last_mode = theme().mode;

  lv_obj_t* footer = make_label(scr, "Вечер и ночь снижают яркость экрана.",
                                &theme().muted14);
  lv_obj_set_pos(footer, 14, 414);
  lv_obj_set_size(footer, 452, 50);
  lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(footer, LV_LABEL_LONG_MODE_WRAP);

  time_picker_create(s.picker);
}

void schedule_screen_update(ScheduleScreen& s, const AppState& state) {
  if (s.screen == nullptr) {
    return;
  }

  NightSchedule effective = state.schedule;

  if (s.values_override) {
    if (same_times(state.schedule, s.values_desired) ||
        lv_tick_elaps(s.values_until) > kOverrideMs) {
      s.values_override = false;
    }
  }
  if (s.values_override) {
    effective.evening_min = s.values_desired.evening_min;
    effective.night_min = s.values_desired.night_min;
    effective.day_min = s.values_desired.day_min;
  }

  if (s.auto_override) {
    if (state.schedule.enabled == s.auto_desired ||
        lv_tick_elaps(s.auto_until) > kOverrideMs) {
      s.auto_override = false;
    }
  }
  if (s.auto_override) {
    effective.enabled = s.auto_desired;
  }

  s.schedule = effective;

  char buffer[8];
  format_hhmm(effective.evening_min, buffer, sizeof(buffer));
  set_label_text(s.evening_value, buffer);
  format_hhmm(effective.night_min, buffer, sizeof(buffer));
  set_label_text(s.night_value, buffer);
  format_hhmm(effective.day_min, buffer, sizeof(buffer));
  set_label_text(s.day_value, buffer);

  /* Do not pull the switch back while the state round-trip is in flight. */
  if (!s.auto_override &&
      !lv_obj_has_state(s.auto_switch, LV_STATE_PRESSED)) {
    const bool checked = lv_obj_has_state(s.auto_switch, LV_STATE_CHECKED);
    if (checked != effective.enabled) {
      s.syncing_switch = true;
      if (effective.enabled) {
        lv_obj_add_state(s.auto_switch, LV_STATE_CHECKED);
      } else {
        lv_obj_remove_state(s.auto_switch, LV_STATE_CHECKED);
      }
      s.syncing_switch = false;
    }
  }

  char status_buffer[32];
  std::snprintf(status_buffer, sizeof(status_buffer), "Сейчас: %s",
                mode_title(state.display_mode));
  set_label_text(s.status, status_buffer);

  if (!night_schedule_valid(effective)) {
    set_label_text(s.warning,
                   "Проверьте порядок: день < вечер < ночь.");
    set_hidden(s.warning, false);
  } else {
    set_hidden(s.warning, true);
  }

  if (s.last_mode != theme().mode) {
    s.last_mode = theme().mode;
    lv_obj_set_style_text_color(s.warning, palette().amber, 0);
    lv_obj_set_style_text_color(s.picker.error, palette().red, 0);
  }
}

}  // namespace ui
}  // namespace tablo
