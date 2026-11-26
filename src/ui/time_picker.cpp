#include "time_picker.h"

#include <cstddef>
#include <cstdio>

#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

constexpr uint16_t kMinutesPerDay = 24U * 60U;

void build_options(char* out, size_t cap, int count, int step) {
  if (out == nullptr || cap == 0) {
    return;
  }
  size_t used = 0;
  out[0] = '\0';
  for (int i = 0; i < count; ++i) {
    const size_t left = cap - used;
    const int written = std::snprintf(out + used, left,
                                      i == 0 ? "%02d" : "\n%02d", i * step);
    if (written < 0 || static_cast<size_t>(written) >= left) {
      out[used] = '\0';
      return;
    }
    used += static_cast<size_t>(written);
  }
}

void style_roller(lv_obj_t* roller) {
  lv_obj_add_style(roller, &theme().text28, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);

  lv_obj_add_style(roller, &theme().text28, LV_PART_SELECTED);
  lv_obj_add_style(roller, &theme().button_primary, LV_PART_SELECTED);
  lv_obj_set_style_pad_ver(roller, 8, LV_PART_SELECTED);
}

void cancel_cb(lv_event_t* event) {
  auto* picker = static_cast<TimePicker*>(lv_event_get_user_data(event));
  if (picker != nullptr) {
    time_picker_hide(*picker);
  }
}

void apply_cb(lv_event_t* event) {
  auto* picker = static_cast<TimePicker*>(lv_event_get_user_data(event));
  if (picker == nullptr || !picker->visible || picker->apply_cb == nullptr) {
    return;
  }
  const uint16_t minutes = time_picker_minutes(*picker);
  const char* error = picker->apply_cb(picker->apply_user_data, minutes);
  if (error != nullptr && error[0] != '\0') {
    time_picker_set_error(*picker, error);
    return;
  }
  time_picker_set_error(*picker, nullptr);
  time_picker_hide(*picker);
}

void roller_changed_cb(lv_event_t* event) {
  auto* picker = static_cast<TimePicker*>(lv_event_get_user_data(event));
  if (picker != nullptr) {
    time_picker_set_error(*picker, nullptr);
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

}  // namespace

void time_picker_create(TimePicker& p) {
  p.overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(p.overlay);
  lv_obj_set_pos(p.overlay, 0, 0);
  lv_obj_set_size(p.overlay, 480, 480);
  lv_obj_set_style_bg_color(p.overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(p.overlay, LV_OPA_70, 0);
  lv_obj_add_flag(p.overlay, LV_OBJ_FLAG_CLICKABLE);
  remove_flags(p.overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(p.overlay, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(p.overlay, LV_OBJ_FLAG_HIDDEN);

  p.panel = lv_obj_create(p.overlay);
  lv_obj_remove_style_all(p.panel);
  lv_obj_add_style(p.panel, &theme().modal, 0);
  lv_obj_set_size(p.panel, 400, 372);
  lv_obj_center(p.panel);
  remove_flags(p.panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(p.panel, LV_SCROLLBAR_MODE_OFF);

  p.title = make_label(p.panel, "Время", &theme().text28);
  lv_obj_set_pos(p.title, 20, 14);
  lv_obj_set_size(p.title, 360, 36);
  lv_obj_set_style_text_align(p.title, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(p.title, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* subtitle = make_label(p.panel, "Время переключения",
                                  &theme().muted14);
  lv_obj_set_pos(subtitle, 20, 52);
  lv_obj_set_size(subtitle, 360, 18);
  lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* hours_caption = make_label(p.panel, "Часы", &theme().muted14);
  lv_obj_set_pos(hours_caption, 66, 78);
  lv_obj_set_size(hours_caption, 112, 18);
  lv_obj_set_style_text_align(hours_caption, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* minutes_caption = make_label(p.panel, "Минуты", &theme().muted14);
  lv_obj_set_pos(minutes_caption, 222, 78);
  lv_obj_set_size(minutes_caption, 112, 18);
  lv_obj_set_style_text_align(minutes_caption, LV_TEXT_ALIGN_CENTER, 0);

  char hours[96];
  char minutes[48];
  build_options(hours, sizeof(hours), 24, 1);
  build_options(minutes, sizeof(minutes), 12, 5);

  p.hour_roller = lv_roller_create(p.panel);
  lv_obj_set_pos(p.hour_roller, 66, 100);
  lv_roller_set_options(p.hour_roller, hours, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(p.hour_roller, 3);
  lv_obj_set_size(p.hour_roller, 112, 152);
  lv_roller_set_selected(p.hour_roller, 0, LV_ANIM_OFF);
  style_roller(p.hour_roller);
  lv_obj_add_event_cb(p.hour_roller, roller_changed_cb, LV_EVENT_VALUE_CHANGED,
                      &p);

  lv_obj_t* colon = make_label(p.panel, ":", &theme().text28);
  lv_obj_set_pos(colon, 178, 148);
  lv_obj_set_size(colon, 44, 36);
  lv_obj_set_style_text_align(colon, LV_TEXT_ALIGN_CENTER, 0);

  p.minute_roller = lv_roller_create(p.panel);
  lv_obj_set_pos(p.minute_roller, 222, 100);
  lv_roller_set_options(p.minute_roller, minutes, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(p.minute_roller, 3);
  lv_obj_set_size(p.minute_roller, 112, 152);
  lv_roller_set_selected(p.minute_roller, 0, LV_ANIM_OFF);
  style_roller(p.minute_roller);
  lv_obj_add_event_cb(p.minute_roller, roller_changed_cb,
                      LV_EVENT_VALUE_CHANGED, &p);

  p.error = make_label(p.panel, "", &theme().text14);
  lv_obj_set_pos(p.error, 20, 256);
  lv_obj_set_size(p.error, 360, 40);
  lv_obj_set_style_text_align(p.error, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(p.error, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_add_flag(p.error, LV_OBJ_FLAG_HIDDEN);

  p.cancel_button = lv_button_create(p.panel);
  lv_obj_set_pos(p.cancel_button, 28, 304);
  lv_obj_set_size(p.cancel_button, 160, 44);
  make_ghost_button(p.cancel_button);
  lv_obj_add_event_cb(p.cancel_button, cancel_cb, LV_EVENT_CLICKED, &p);

  lv_obj_t* cancel_label = make_label(p.cancel_button, "Отмена",
                                      &theme().text18);
  lv_obj_set_size(cancel_label, 154, 24);
  lv_obj_set_style_text_align(cancel_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(cancel_label);

  p.apply_button = lv_button_create(p.panel);
  lv_obj_set_pos(p.apply_button, 212, 304);
  lv_obj_set_size(p.apply_button, 160, 44);
  make_primary_button(p.apply_button);
  lv_obj_add_event_cb(p.apply_button, apply_cb, LV_EVENT_CLICKED, &p);

  lv_obj_t* apply_label = make_label(p.apply_button, "Готово", nullptr);
  lv_obj_set_style_text_font(apply_label, &ui_font_18, 0);
  lv_obj_set_style_text_color(apply_label, palette().dark_text, 0);
  lv_obj_set_size(apply_label, 154, 24);
  lv_obj_set_style_text_align(apply_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(apply_label);
}

void time_picker_show(TimePicker& p, const char* title, uint16_t minutes,
                      TimePickerApplyCb apply_cb, void* apply_user_data) {
  if (p.overlay == nullptr) {
    return;
  }
  if (minutes >= kMinutesPerDay) {
    minutes = 0;
  }

  p.apply_cb = apply_cb;
  p.apply_user_data = apply_user_data;

  set_label_text(p.title, title != nullptr && title[0] != '\0' ? title
                                                               : "Время");
  time_picker_set_error(p, nullptr);
  lv_obj_set_style_text_color(p.error, palette().red, 0);

  lv_roller_set_selected(p.hour_roller,
                         static_cast<uint32_t>(minutes / 60U), LV_ANIM_OFF);
  lv_roller_set_selected(p.minute_roller,
                         static_cast<uint32_t>((minutes % 60U) / 5U),
                         LV_ANIM_OFF);

  remove_flags(p.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(p.overlay, -1);
  p.visible = true;
}

void time_picker_hide(TimePicker& p) {
  if (p.overlay == nullptr) {
    return;
  }
  lv_obj_add_flag(p.overlay, LV_OBJ_FLAG_HIDDEN);
  p.visible = false;
  p.apply_cb = nullptr;
  p.apply_user_data = nullptr;
}

void time_picker_set_error(TimePicker& p, const char* message) {
  if (p.error == nullptr) {
    return;
  }
  if (message != nullptr && message[0] != '\0') {
    set_label_text_safe(p.error, message);
    remove_flags(p.error, LV_OBJ_FLAG_HIDDEN);
  } else {
    set_label_text(p.error, "");
    lv_obj_add_flag(p.error, LV_OBJ_FLAG_HIDDEN);
  }
}

uint16_t time_picker_minutes(const TimePicker& p) {
  if (p.hour_roller == nullptr || p.minute_roller == nullptr) {
    return 0;
  }
  uint32_t hour = lv_roller_get_selected(p.hour_roller);
  uint32_t slot = lv_roller_get_selected(p.minute_roller);
  if (hour > 23U) {
    hour = 23U;
  }
  if (slot > 11U) {
    slot = 11U;
  }
  return static_cast<uint16_t>(hour * 60U + slot * 5U);
}

}  // namespace ui
}  // namespace tablo
