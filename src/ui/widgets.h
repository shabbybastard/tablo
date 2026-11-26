#pragma once

#include <cstddef>
#include <cstdint>

#include <lvgl.h>

namespace tablo {
namespace ui {

lv_obj_t* make_label(lv_obj_t* parent, const char* text, const lv_style_t* style);
void set_label_text(lv_obj_t* label, const char* text);
void set_label_text_safe(lv_obj_t* label, const char* text);
void sanitize_text(const char* in, char* out, size_t out_size);

lv_obj_t* make_card(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h);
lv_obj_t* make_divider(lv_obj_t* parent, int32_t x, int32_t y, int32_t w);
lv_obj_t* make_dot(lv_obj_t* parent, int32_t x, int32_t y, int32_t size,
                   lv_color_t color);

void set_pad_all(lv_obj_t* obj, int32_t pad);
void make_non_interactive(lv_obj_t* obj);
void remove_flags(lv_obj_t* obj, uint32_t flags);

/** "2 д 4 ч", "3 ч 12 мин", "42 мин". */
void format_uptime(uint32_t seconds, char* out, size_t out_size);

lv_color_t aqi_color(int aqi);
const char* aqi_text(int aqi);

}  // namespace ui
}  // namespace tablo
