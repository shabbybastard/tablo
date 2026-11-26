#pragma once

#include <lvgl.h>

namespace tablo {

bool display_begin();
void display_show_message(const char* line1, const char* line2 = nullptr,
                          const char* line3 = nullptr);
lv_display_t* display_lvgl_create();
bool display_lvgl_active();

}  // namespace tablo
