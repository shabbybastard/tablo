#pragma once

#include <lvgl.h>

#include "core/schedule.h"

namespace tablo {
namespace ui {

struct Palette {
  lv_color_t bg;
  lv_color_t status_bg;
  lv_color_t panel;
  lv_color_t panel_alt;
  lv_color_t border;
  lv_color_t text;
  lv_color_t muted;
  lv_color_t accent;
  lv_color_t green;
  lv_color_t red;
  lv_color_t amber;
  lv_color_t blue;
  lv_color_t dark_text;
};

struct Theme {
  DisplayMode mode = DisplayMode::Day;
  bool night = false;
  Palette palette{};

  lv_style_t screen;
  lv_style_t status;
  lv_style_t card;
  lv_style_t divider;

  lv_style_t text14;
  lv_style_t text18;
  lv_style_t text28;
  lv_style_t muted14;
  lv_style_t muted18;
  lv_style_t accent14;
  lv_style_t accent18;
  lv_style_t accent28;
  lv_style_t clock48;

  lv_style_t pill;
  lv_style_t dot_active;
  lv_style_t dot_inactive;
  lv_style_t modal;

  lv_style_t button_primary;
  lv_style_t button_primary_pressed;
  lv_style_t button_ghost;
  lv_style_t button_ghost_pressed;

  lv_style_t slider_track;
  lv_style_t slider_indicator;
  lv_style_t slider_knob;

  lv_style_t switch_track;
  lv_style_t switch_indicator_off;
  lv_style_t switch_indicator_on;
  lv_style_t switch_knob;

  lv_style_t chart;
};

/** Initialize style objects once, before any screen is built. */
void theme_init();

/** Swap the palette. Safe to call while the UI is running. */
void theme_apply(DisplayMode mode);

Theme& theme();
const Palette& palette();

}  // namespace ui
}  // namespace tablo
