#pragma once

#include <lvgl.h>

namespace tablo {

bool touch_begin();
lv_indev_t* touch_lvgl_create();

}  // namespace tablo
