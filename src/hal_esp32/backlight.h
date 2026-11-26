#pragma once

#include <cstdint>

namespace tablo {

void backlight_init();
void backlight_set(uint8_t percent);

/** Recompute PWM from the current AppState brightness/display mode. */
void apply_current_backlight();

}  // namespace tablo
