#pragma once

#include <lvgl.h>

namespace tablo {
namespace ui {

/** Row of four settings tabs; `active` is 0..3. */
void settings_tabs_create(lv_obj_t* parent, int active);

/** Switch between the settings pages: 0 basic, 1 local, 2 device, 3 external. */
void ui_show_settings_tab(int tab);

}  // namespace ui
}  // namespace tablo
