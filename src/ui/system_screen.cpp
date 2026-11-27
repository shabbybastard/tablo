#include "system_screen.h"

#include <cstdio>

#include "core/platform.h"
#include "fonts/fonts.h"
#include "settings_tabs.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

const char* wifi_quality(int8_t rssi) {
  if (rssi >= -55) return "Отлично";
  if (rssi >= -67) return "Хорошо";
  if (rssi >= -75) return "Средне";
  return "Слабо";
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

void check_updates_cb(lv_event_t*) { platform().checkForUpdates(); }

void reboot_cb(lv_event_t*) { platform().reboot(); }

lv_obj_t* make_row(lv_obj_t* parent, int32_t y, const char* caption,
                   lv_obj_t** value_out) {
  lv_obj_t* label = make_label(parent, caption, &theme().muted14);
  lv_obj_set_pos(label, 16, y);
  lv_obj_set_size(label, 130, 20);
  *value_out = make_label(parent, "-", &theme().accent14);
  lv_obj_set_pos(*value_out, 150, y - 1);
  lv_obj_set_size(*value_out, 258, 22);
  lv_obj_set_style_text_align(*value_out, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(*value_out, LV_LABEL_LONG_MODE_DOTS);
  return label;
}

}  // namespace

void system_screen_create(SystemScreen& s, lv_event_cb_t version_cb,
                          lv_event_cb_t menu_cb) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  remove_flags(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  s.screen = scr;

  status_bar_create(s.status, scr, "←", menu_cb, version_cb);

  settings_tabs_create(scr, 2);

  lv_obj_t* title = make_label(scr, "Устройство и сеть", &theme().text28);
  lv_obj_set_pos(title, 14, 94);
  lv_obj_set_size(title, 452, 32);

  lv_obj_t* panel = make_card(scr, 12, 134, 456, 286);

  lv_obj_t* ip_caption = make_label(panel, "IP-адрес", &theme().muted14);
  lv_obj_set_pos(ip_caption, 16, 42);
  lv_obj_set_size(ip_caption, 130, 20);
  s.ip_value = make_label(panel, "-", &theme().text18);
  lv_obj_set_pos(s.ip_value, 150, 38);
  lv_obj_set_size(s.ip_value, 258, 24);
  lv_obj_set_style_text_align(s.ip_value, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(s.ip_value, LV_LABEL_LONG_MODE_DOTS);

  make_divider(panel, 16, 72, 424);

  make_row(panel, 86, "Wi-Fi", &s.wifi_value);
  make_row(panel, 114, "CPU", &s.cpu_value);
  make_row(panel, 142, "RAM", &s.ram_value);
  make_row(panel, 170, "Flash", &s.flash_value);
  make_row(panel, 198, "Аптайм", &s.uptime_value);

  s.check_button = lv_button_create(panel);
  lv_obj_set_pos(s.check_button, 16, 232);
  lv_obj_set_size(s.check_button, 200, 42);
  make_primary_button(s.check_button);
  lv_obj_add_event_cb(s.check_button, check_updates_cb, LV_EVENT_CLICKED, &s);
  lv_obj_t* check_label = make_label(s.check_button, "Проверить обновления",
                                      nullptr);
  lv_obj_set_style_text_font(check_label, &ui_font_14, 0);
  lv_obj_set_style_text_color(check_label, palette().dark_text, 0);
  lv_obj_set_size(check_label, 194, 24);
  lv_obj_set_style_text_align(check_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(check_label, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_center(check_label);

  s.reboot_button = lv_button_create(panel);
  lv_obj_set_pos(s.reboot_button, 232, 232);
  lv_obj_set_size(s.reboot_button, 200, 42);
  make_ghost_button(s.reboot_button);
  lv_obj_add_event_cb(s.reboot_button, reboot_cb, LV_EVENT_CLICKED, &s);
  lv_obj_t* reboot_label = make_label(s.reboot_button, "Перезагрузить",
                                       &theme().text18);
  lv_obj_set_size(reboot_label, 194, 24);
  lv_obj_set_style_text_align(reboot_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(reboot_label, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_center(reboot_label);

  s.footer = make_label(scr, "", &theme().muted14);
  lv_obj_set_pos(s.footer, 12, 430);
  lv_obj_set_size(s.footer, 456, 20);
  lv_obj_set_style_text_align(s.footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(s.footer, LV_LABEL_LONG_MODE_DOTS);
}

void system_screen_update(SystemScreen& s, const AppState& state) {
  status_bar_update(s.status, state);

  char buffer[96];
  if (state.ip[0] != '\0') {
    std::snprintf(buffer, sizeof(buffer), "%s", state.ip);
  } else {
    std::snprintf(buffer, sizeof(buffer), "-");
  }
  set_label_text_safe(s.ip_value, buffer);

  if (state.wifi_connected) {
    std::snprintf(buffer, sizeof(buffer), "%s %d",
                  wifi_quality(state.wifi_rssi), static_cast<int>(state.wifi_rssi));
  } else {
    std::snprintf(buffer, sizeof(buffer), "Нет");
  }
  set_label_text(s.wifi_value, buffer);

  if (state.cpu_mhz > 0) {
    std::snprintf(buffer, sizeof(buffer), "%u МГц",
                  static_cast<unsigned>(state.cpu_mhz));
  } else {
    std::snprintf(buffer, sizeof(buffer), "-");
  }
  set_label_text(s.cpu_value, buffer);

  if (state.ram_total > state.ram_free) {
    std::snprintf(buffer, sizeof(buffer), "%lu/%lu КБ",
                  static_cast<unsigned long>((state.ram_total - state.ram_free) /
                                             1024U),
                  static_cast<unsigned long>(state.ram_total / 1024U));
  } else {
    std::snprintf(buffer, sizeof(buffer), "-");
  }
  set_label_text(s.ram_value, buffer);

  if (state.flash_total > 0) {
    std::snprintf(buffer, sizeof(buffer), "%.1f/%.1f МБ",
                  static_cast<double>(state.flash_used) / 1048576.0,
                  static_cast<double>(state.flash_total) / 1048576.0);
  } else {
    std::snprintf(buffer, sizeof(buffer), "-");
  }
  set_label_text(s.flash_value, buffer);

  format_uptime(state.uptime_s, buffer, sizeof(buffer));
  set_label_text(s.uptime_value, buffer);

  if (state.ota.downloading) {
    std::snprintf(buffer, sizeof(buffer), "Загрузка обновления: %d%%",
                  static_cast<int>(state.ota.progress));
  } else if (state.ota.available && state.ota.version[0] != '\0') {
    std::snprintf(buffer, sizeof(buffer), "Доступно обновление %s",
                  state.ota.version);
  } else if (state.ota.status[0] != '\0') {
    std::snprintf(buffer, sizeof(buffer), "%s", state.ota.status);
  } else {
    std::snprintf(buffer, sizeof(buffer), "Данные обновляются автоматически");
  }
  set_label_text_safe(s.footer, buffer);
}

}  // namespace ui
}  // namespace tablo
