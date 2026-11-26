#include "theme.h"

#include "fonts/fonts.h"

namespace tablo {
namespace ui {
namespace {

Theme g_theme;

Palette make_palette(DisplayMode mode) {
  Palette p{};
  if (mode == DisplayMode::Night) {
    p.bg = lv_color_hex(0x03060A);
    p.status_bg = lv_color_hex(0x070B10);
    p.panel = lv_color_hex(0x090E14);
    p.panel_alt = lv_color_hex(0x0D141C);
    p.border = lv_color_hex(0x18232E);
    p.text = lv_color_hex(0xB9C5D2);
    p.muted = lv_color_hex(0x5F6D7B);
    p.accent = lv_color_hex(0x2E93B8);
    p.green = lv_color_hex(0x2FA878);
    p.red = lv_color_hex(0xC85A5A);
    p.amber = lv_color_hex(0xB98A33);
    p.blue = lv_color_hex(0x4F7DB8);
    p.dark_text = lv_color_hex(0x020508);
  } else if (mode == DisplayMode::Evening) {
    p.bg = lv_color_hex(0x120E0A);
    p.status_bg = lv_color_hex(0x17120D);
    p.panel = lv_color_hex(0x1D1711);
    p.panel_alt = lv_color_hex(0x2A2017);
    p.border = lv_color_hex(0x3A2C20);
    p.text = lv_color_hex(0xEADCC9);
    p.muted = lv_color_hex(0x9A8874);
    p.accent = lv_color_hex(0xF0A45C);
    p.green = lv_color_hex(0x9BCB6B);
    p.red = lv_color_hex(0xE57373);
    p.amber = lv_color_hex(0xFFB74D);
    p.blue = lv_color_hex(0x90A4AE);
    p.dark_text = lv_color_hex(0x1A0F06);
  } else {
    p.bg = lv_color_hex(0x0B0F14);
    p.status_bg = lv_color_hex(0x0E141B);
    p.panel = lv_color_hex(0x111821);
    p.panel_alt = lv_color_hex(0x17212B);
    p.border = lv_color_hex(0x22303D);
    p.text = lv_color_hex(0xE9EFF6);
    p.muted = lv_color_hex(0x7C8A9A);
    p.accent = lv_color_hex(0x4FC3F7);
    p.green = lv_color_hex(0x3DDC97);
    p.red = lv_color_hex(0xFF6B6B);
    p.amber = lv_color_hex(0xF5B942);
    p.blue = lv_color_hex(0x6EA8FE);
    p.dark_text = lv_color_hex(0x061018);
  }
  return p;
}

void init_base_styles() {
  Theme& t = g_theme;

  lv_style_init(&t.screen);
  lv_style_init(&t.status);
  lv_style_init(&t.card);
  lv_style_init(&t.divider);

  lv_style_init(&t.text14);
  lv_style_init(&t.text18);
  lv_style_init(&t.text28);
  lv_style_init(&t.muted14);
  lv_style_init(&t.muted18);
  lv_style_init(&t.accent14);
  lv_style_init(&t.accent18);
  lv_style_init(&t.accent28);
  lv_style_init(&t.clock48);

  lv_style_init(&t.pill);
  lv_style_init(&t.dot_active);
  lv_style_init(&t.dot_inactive);
  lv_style_init(&t.modal);

  lv_style_init(&t.button_primary);
  lv_style_init(&t.button_primary_pressed);
  lv_style_init(&t.button_ghost);
  lv_style_init(&t.button_ghost_pressed);

  lv_style_init(&t.slider_track);
  lv_style_init(&t.slider_indicator);
  lv_style_init(&t.slider_knob);

  lv_style_init(&t.switch_track);
  lv_style_init(&t.switch_indicator_off);
  lv_style_init(&t.switch_indicator_on);
  lv_style_init(&t.switch_knob);

  lv_style_init(&t.chart);

  /* Fonts never change at runtime. */
  lv_style_set_text_font(&t.text14, &ui_font_14);
  lv_style_set_text_font(&t.text18, &ui_font_18);
  lv_style_set_text_font(&t.text28, &ui_font_28);
  lv_style_set_text_font(&t.muted14, &ui_font_14);
  lv_style_set_text_font(&t.muted18, &ui_font_18);
  lv_style_set_text_font(&t.accent14, &ui_font_14);
  lv_style_set_text_font(&t.accent18, &ui_font_18);
  lv_style_set_text_font(&t.accent28, &ui_font_28);
  lv_style_set_text_font(&t.clock48, &lv_font_montserrat_48);

  /* Geometry. */
  lv_style_set_radius(&t.screen, 0);
  lv_style_set_border_width(&t.screen, 0);
  lv_style_set_pad_all(&t.screen, 0);

  lv_style_set_radius(&t.status, 0);
  lv_style_set_border_width(&t.status, 0);
  lv_style_set_pad_all(&t.status, 0);

  lv_style_set_radius(&t.card, 18);
  lv_style_set_border_width(&t.card, 1);
  lv_style_set_pad_all(&t.card, 0);
  lv_style_set_shadow_width(&t.card, 18);
  lv_style_set_shadow_offset_y(&t.card, 6);
  lv_style_set_shadow_opa(&t.card, LV_OPA_30);

  lv_style_set_radius(&t.divider, 0);

  lv_style_set_radius(&t.pill, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.pill, 0);
  lv_style_set_pad_all(&t.pill, 0);

  lv_style_set_radius(&t.dot_active, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.dot_active, 0);
  lv_style_set_radius(&t.dot_inactive, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.dot_inactive, 0);

  lv_style_set_radius(&t.modal, 20);
  lv_style_set_border_width(&t.modal, 1);
  lv_style_set_pad_all(&t.modal, 0);
  lv_style_set_shadow_width(&t.modal, 30);
  lv_style_set_shadow_offset_y(&t.modal, 10);
  lv_style_set_shadow_opa(&t.modal, LV_OPA_60);

  lv_style_set_radius(&t.button_primary, 12);
  lv_style_set_border_width(&t.button_primary, 0);
  lv_style_set_pad_all(&t.button_primary, 0);
  lv_style_set_radius(&t.button_primary_pressed, 12);
  lv_style_set_border_width(&t.button_primary_pressed, 0);
  lv_style_set_pad_all(&t.button_primary_pressed, 0);

  lv_style_set_radius(&t.button_ghost, 12);
  lv_style_set_border_width(&t.button_ghost, 1);
  lv_style_set_pad_all(&t.button_ghost, 0);
  lv_style_set_radius(&t.button_ghost_pressed, 12);
  lv_style_set_border_width(&t.button_ghost_pressed, 1);
  lv_style_set_pad_all(&t.button_ghost_pressed, 0);

  lv_style_set_radius(&t.slider_track, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.slider_track, 0);
  lv_style_set_radius(&t.slider_indicator, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.slider_indicator, 0);
  lv_style_set_radius(&t.slider_knob, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.slider_knob, 0);
  lv_style_set_pad_all(&t.slider_knob, 5);

  lv_style_set_radius(&t.switch_track, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.switch_track, 1);
  lv_style_set_radius(&t.switch_indicator_off, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.switch_indicator_off, 0);
  lv_style_set_radius(&t.switch_indicator_on, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.switch_indicator_on, 0);
  lv_style_set_radius(&t.switch_knob, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&t.switch_knob, 0);
  lv_style_set_pad_all(&t.switch_knob, 3);

  lv_style_set_radius(&t.chart, 8);
  lv_style_set_border_width(&t.chart, 0);
  lv_style_set_pad_all(&t.chart, 0);
  lv_style_set_bg_opa(&t.chart, LV_OPA_TRANSP);
  lv_style_set_line_width(&t.chart, 1);
}

}  // namespace

void theme_init() {
  init_base_styles();
  theme_apply(DisplayMode::Day);
}

void theme_apply(DisplayMode mode) {
  Theme& t = g_theme;
  t.mode = mode;
  t.night = mode == DisplayMode::Night;
  t.palette = make_palette(mode);
  const Palette& p = t.palette;

  lv_style_set_bg_color(&t.screen, p.bg);
  lv_style_set_bg_opa(&t.screen, LV_OPA_COVER);
  lv_style_set_text_color(&t.screen, p.text);

  lv_style_set_bg_color(&t.status, p.status_bg);
  lv_style_set_bg_opa(&t.status, LV_OPA_COVER);
  lv_style_set_text_color(&t.status, p.text);

  lv_style_set_bg_color(&t.card, p.panel);
  lv_style_set_bg_opa(&t.card, LV_OPA_COVER);
  lv_style_set_border_color(&t.card, p.border);
  lv_style_set_shadow_color(&t.card, lv_color_black());
  lv_style_set_text_color(&t.card, p.text);

  lv_style_set_bg_color(&t.divider, p.border);
  lv_style_set_bg_opa(&t.divider, LV_OPA_COVER);

  lv_style_set_text_color(&t.text14, p.text);
  lv_style_set_text_color(&t.text18, p.text);
  lv_style_set_text_color(&t.text28, p.text);
  lv_style_set_text_color(&t.muted14, p.muted);
  lv_style_set_text_color(&t.muted18, p.muted);
  lv_style_set_text_color(&t.accent14, p.accent);
  lv_style_set_text_color(&t.accent18, p.accent);
  lv_style_set_text_color(&t.accent28, p.accent);
  lv_style_set_text_color(&t.clock48, p.text);

  lv_style_set_bg_color(&t.pill, p.panel_alt);
  lv_style_set_bg_opa(&t.pill, LV_OPA_COVER);
  lv_style_set_text_color(&t.pill, p.text);

  lv_style_set_bg_color(&t.dot_active, p.accent);
  lv_style_set_bg_opa(&t.dot_active, LV_OPA_COVER);
  lv_style_set_bg_color(&t.dot_inactive, p.muted);
  lv_style_set_bg_opa(&t.dot_inactive, LV_OPA_40);

  lv_style_set_bg_color(&t.modal, p.panel_alt);
  lv_style_set_bg_opa(&t.modal, LV_OPA_COVER);
  lv_style_set_border_color(&t.modal, p.border);
  lv_style_set_shadow_color(&t.modal, lv_color_black());
  lv_style_set_text_color(&t.modal, p.text);

  lv_style_set_bg_color(&t.button_primary, p.accent);
  lv_style_set_bg_opa(&t.button_primary, LV_OPA_COVER);
  lv_style_set_text_color(&t.button_primary, p.dark_text);
  lv_style_set_bg_color(&t.button_primary_pressed, lv_color_lighten(p.accent, 28));
  lv_style_set_bg_opa(&t.button_primary_pressed, LV_OPA_COVER);

  lv_style_set_bg_color(&t.button_ghost, p.panel_alt);
  lv_style_set_bg_opa(&t.button_ghost, LV_OPA_COVER);
  lv_style_set_border_color(&t.button_ghost, p.border);
  lv_style_set_text_color(&t.button_ghost, p.text);
  lv_style_set_bg_color(&t.button_ghost_pressed, lv_color_lighten(p.panel_alt, 22));
  lv_style_set_bg_opa(&t.button_ghost_pressed, LV_OPA_COVER);
  lv_style_set_border_color(&t.button_ghost_pressed, p.border);

  lv_style_set_bg_color(&t.slider_track, p.panel_alt);
  lv_style_set_bg_opa(&t.slider_track, LV_OPA_COVER);
  lv_style_set_bg_color(&t.slider_indicator, p.accent);
  lv_style_set_bg_opa(&t.slider_indicator, LV_OPA_COVER);
  lv_style_set_bg_color(&t.slider_knob, p.text);
  lv_style_set_bg_opa(&t.slider_knob, LV_OPA_COVER);

  lv_style_set_bg_color(&t.switch_track, p.panel_alt);
  lv_style_set_bg_opa(&t.switch_track, LV_OPA_COVER);
  lv_style_set_border_color(&t.switch_track, p.border);
  lv_style_set_bg_opa(&t.switch_indicator_off, LV_OPA_TRANSP);
  lv_style_set_bg_color(&t.switch_indicator_on, p.green);
  lv_style_set_bg_opa(&t.switch_indicator_on, LV_OPA_COVER);
  lv_style_set_bg_color(&t.switch_knob, p.text);
  lv_style_set_bg_opa(&t.switch_knob, LV_OPA_COVER);

  lv_style_set_line_color(&t.chart, p.border);

  /* Styles are shared by many widgets; notify everything at once. */
  lv_obj_report_style_change(nullptr);
}

Theme& theme() {
  return g_theme;
}

const Palette& palette() {
  return g_theme.palette;
}

}  // namespace ui
}  // namespace tablo
