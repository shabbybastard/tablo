#include "backlight.h"

#include <Arduino.h>

#include <mutex>

#include "board_pins.h"
#include "core/schedule.h"
#include "core/state_store.h"

namespace tablo {
namespace {

bool s_attached = false;
std::mutex s_mutex;
std::mutex s_apply_mutex;

constexpr uint32_t kMaxDuty = (1u << board::LCD_BACKLIGHT_BITS) - 1u;  // 1023

uint32_t percent_to_duty(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  return static_cast<uint32_t>(percent) * kMaxDuty / 100u;
}

}  // namespace

void backlight_init() {
  s_attached = ledcAttach(board::LCD_BACKLIGHT, board::LCD_BACKLIGHT_HZ,
                          board::LCD_BACKLIGHT_BITS);
  if (!s_attached) {
    Serial.printf("[backlight] ledcAttach failed on GPIO%d\n",
                  board::LCD_BACKLIGHT);
    return;
  }

  ledcWrite(board::LCD_BACKLIGHT, 0);  // Keep the panel dark until asked.
  Serial.printf("[backlight] PWM on GPIO%d, %d Hz, %d-bit\n",
                board::LCD_BACKLIGHT, board::LCD_BACKLIGHT_HZ,
                board::LCD_BACKLIGHT_BITS);
}

void backlight_set(uint8_t percent) {
  std::lock_guard<std::mutex> lock(s_mutex);
  if (!s_attached) {
    backlight_init();
    if (!s_attached) {
      return;
    }
  }

  // 0 % is fully off, 100 % is full duty (1023 at 10-bit resolution).
  ledcWrite(board::LCD_BACKLIGHT, percent_to_duty(percent));
}

void apply_current_backlight() {
  std::lock_guard<std::mutex> lock(s_apply_mutex);
  const AppState state = g_state.snapshot();
  backlight_set(brightness_for_mode(state.brightness, state.display_mode));
}

}  // namespace tablo
