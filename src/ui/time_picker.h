#pragma once

#include <cstdint>

#include <lvgl.h>

namespace tablo {
namespace ui {

/**
 * Called when "Готово" is pressed with minutes since midnight.
 *
 * Return nullptr to accept the value and close the picker, or a user-facing
 * error message to keep the picker open and show the message inside it.
 */
using TimePickerApplyCb = const char* (*)(void* user_data, uint16_t minutes);

/** Modal hour/minute picker with 5-minute steps. */
struct TimePicker {
  lv_obj_t* overlay = nullptr;
  lv_obj_t* panel = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* hour_roller = nullptr;
  lv_obj_t* minute_roller = nullptr;
  lv_obj_t* error = nullptr;
  lv_obj_t* cancel_button = nullptr;
  lv_obj_t* apply_button = nullptr;

  TimePickerApplyCb apply_cb = nullptr;
  void* apply_user_data = nullptr;
  bool visible = false;
};

void time_picker_create(TimePicker& picker);
void time_picker_show(TimePicker& picker, const char* title, uint16_t minutes,
                      TimePickerApplyCb apply_cb, void* apply_user_data);
void time_picker_hide(TimePicker& picker);
void time_picker_set_error(TimePicker& picker, const char* message);
uint16_t time_picker_minutes(const TimePicker& picker);

}  // namespace ui
}  // namespace tablo
