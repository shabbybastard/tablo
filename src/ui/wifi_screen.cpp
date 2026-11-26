#include "wifi_screen.h"

#include <cstdio>
#include <cstring>

#include "core/platform.h"
#include "core/state_store.h"
#include "fonts/fonts.h"
#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

void style_roller(lv_obj_t* roller) {
  lv_obj_add_style(roller, &theme().text18, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);
  lv_obj_add_style(roller, &theme().text18, LV_PART_SELECTED);
  lv_obj_add_style(roller, &theme().button_primary, LV_PART_SELECTED);
  lv_obj_set_style_pad_ver(roller, 6, LV_PART_SELECTED);
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

void make_primary_label(lv_obj_t* button, const char* text) {
  lv_obj_t* label = make_label(button, text, nullptr);
  lv_obj_set_style_text_font(label, &ui_font_18, 0);
  lv_obj_set_style_text_color(label, palette().dark_text, 0);
  lv_obj_set_size(label, 190, 24);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);
}

struct PasswordModal {
  WifiScreen* screen = nullptr;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* textarea = nullptr;
  char ssid[33] = "";
};

void close_password(PasswordModal* modal) {
  if (modal == nullptr) return;
  if (modal->screen != nullptr) modal->screen->password_modal = nullptr;
  if (modal->overlay != nullptr) lv_obj_delete(modal->overlay);
  delete modal;
}

void password_cancel_cb(lv_event_t* event) {
  close_password(static_cast<PasswordModal*>(lv_event_get_user_data(event)));
}

void password_connect_cb(lv_event_t* event) {
  auto* modal = static_cast<PasswordModal*>(lv_event_get_user_data(event));
  if (modal == nullptr || modal->textarea == nullptr) return;
  char ssid[sizeof(modal->ssid)] = {};
  char password[65] = {};
  std::snprintf(ssid, sizeof(ssid), "%s", modal->ssid);
  std::snprintf(password, sizeof(password), "%s",
                lv_textarea_get_text(modal->textarea));
  close_password(modal);
  platform().connectWifi(ssid, password);
}

void open_password(WifiScreen& screen, const char* ssid) {
  if (screen.password_modal != nullptr || ssid == nullptr) return;
  auto* modal = new PasswordModal();
  modal->screen = &screen;
  std::snprintf(modal->ssid, sizeof(modal->ssid), "%s", ssid);
  screen.password_modal = modal;

  modal->overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(modal->overlay);
  lv_obj_set_pos(modal->overlay, 0, 0);
  lv_obj_set_size(modal->overlay, 480, 480);
  lv_obj_set_style_bg_color(modal->overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(modal->overlay, LV_OPA_60, 0);
  remove_flags(modal->overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(modal->overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* panel = make_card(modal->overlay, 10, 10, 460, 460);
  lv_obj_t* title = make_label(panel, "Пароль Wi-Fi", &theme().text28);
  lv_obj_set_pos(title, 18, 10);
  lv_obj_set_size(title, 300, 36);

  char subtitle[64] = {};
  std::snprintf(subtitle, sizeof(subtitle), "%s", modal->ssid);
  lv_obj_t* ssid_label = make_label(panel, subtitle, &theme().muted14);
  lv_obj_set_pos(ssid_label, 18, 46);
  lv_obj_set_size(ssid_label, 424, 20);
  lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_MODE_DOTS);

  modal->textarea = lv_textarea_create(panel);
  lv_obj_set_pos(modal->textarea, 18, 74);
  lv_obj_set_size(modal->textarea, 424, 48);
  lv_textarea_set_one_line(modal->textarea, true);
  lv_textarea_set_password_mode(modal->textarea, true);
  lv_textarea_set_max_length(modal->textarea, 64);
  lv_textarea_set_placeholder_text(modal->textarea, "Пароль");
  lv_obj_add_style(modal->textarea, &theme().text18, 0);

  lv_obj_t* keyboard = lv_keyboard_create(panel);
  lv_obj_set_pos(keyboard, 10, 132);
  lv_obj_set_size(keyboard, 440, 260);
  lv_keyboard_set_textarea(keyboard, modal->textarea);

  lv_obj_t* cancel = lv_button_create(panel);
  lv_obj_set_pos(cancel, 18, 404);
  lv_obj_set_size(cancel, 200, 42);
  make_ghost_button(cancel);
  lv_obj_add_event_cb(cancel, password_cancel_cb, LV_EVENT_CLICKED, modal);
  lv_obj_t* cancel_label = make_label(cancel, "Отмена", &theme().text18);
  lv_obj_set_size(cancel_label, 190, 24);
  lv_obj_set_style_text_align(cancel_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(cancel_label);

  lv_obj_t* connect = lv_button_create(panel);
  lv_obj_set_pos(connect, 242, 404);
  lv_obj_set_size(connect, 200, 42);
  make_primary_button(connect);
  lv_obj_add_event_cb(connect, password_connect_cb, LV_EVENT_CLICKED, modal);
  make_primary_label(connect, "Подключить");
}

void connect_cb(lv_event_t* event) {
  auto* screen = static_cast<WifiScreen*>(lv_event_get_user_data(event));
  if (screen == nullptr || screen->network_roller == nullptr) return;
  const AppState state = g_state.snapshot();
  const uint16_t index = lv_roller_get_selected(screen->network_roller);
  if (index >= state.wifi_scan.count) return;
  const WifiScanEntry& entry = state.wifi_scan.entries[index];
  if (entry.secure) {
    open_password(*screen, entry.ssid);
  } else {
    platform().connectWifi(entry.ssid, "");
  }
}

void refresh_cb(lv_event_t* event) {
  auto* screen = static_cast<WifiScreen*>(lv_event_get_user_data(event));
  if (screen != nullptr) wifi_refresh(*screen);
}

void rebuild_networks(WifiScreen& wifi, const AppState& state) {
  char options[1200] = {};
  size_t used = 0;
  const uint8_t count = state.wifi_scan.count;
  if (count == 0) {
    std::snprintf(options, sizeof(options), "Сети не найдены");
  } else {
    for (uint8_t i = 0; i < count && i < kMaxWifiScan; ++i) {
      char ssid[64] = {};
      sanitize_text(state.wifi_scan.entries[i].ssid, ssid, sizeof(ssid));
      if (ssid[0] == '\0') {
        std::snprintf(ssid, sizeof(ssid), "Без имени");
      }
      const int written = std::snprintf(
          options + used, sizeof(options) - used, i == 0 ? "%s  %d dBm%s" : "\n%s  %d dBm%s",
          ssid, static_cast<int>(state.wifi_scan.entries[i].rssi),
          state.wifi_scan.entries[i].secure ? "  *" : "");
      if (written < 0 || static_cast<size_t>(written) >= sizeof(options) - used) {
        options[used] = '\0';
        break;
      }
      used += static_cast<size_t>(written);
    }
  }
  const uint16_t selected = lv_roller_get_selected(wifi.network_roller);
  lv_roller_set_options(wifi.network_roller, options, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(wifi.network_roller, 3);
  if (count > 0) {
    lv_roller_set_selected(wifi.network_roller,
                           selected < count ? selected : 0, LV_ANIM_OFF);
  }
  if (count == 0) {
    lv_obj_add_state(wifi.connect_button, LV_STATE_DISABLED);
  } else {
    lv_obj_remove_state(wifi.connect_button, LV_STATE_DISABLED);
  }
  wifi.last_generation = state.wifi_scan.generation;
  wifi.last_count = count;
}

}  // namespace

void wifi_refresh(WifiScreen& wifi) {
  set_label_text(wifi.status_label, "Сканирование...");
  set_label_text(wifi.footer, "Поиск сетей Wi-Fi");
  lv_obj_add_state(wifi.connect_button, LV_STATE_DISABLED);
  platform().requestWifiScan();
}

void wifi_create(WifiScreen& wifi, lv_event_cb_t version_cb,
                 lv_event_cb_t menu_cb) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  remove_flags(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  wifi.screen = scr;

  status_bar_create(wifi.status, scr, "Назад", menu_cb, version_cb);

  lv_obj_t* title = make_label(scr, "Wi-Fi", &theme().text28);
  lv_obj_set_pos(title, 14, 48);
  lv_obj_set_size(title, 452, 38);

  wifi.current_label = make_label(scr, "Сейчас: не задана", &theme().muted14);
  lv_obj_set_pos(wifi.current_label, 14, 86);
  lv_obj_set_size(wifi.current_label, 452, 18);
  lv_label_set_long_mode(wifi.current_label, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* panel = make_card(scr, 12, 108, 456, 286);
  lv_obj_t* caption = make_label(panel, "ДОСТУПНЫЕ СЕТИ", &theme().muted14);
  lv_obj_set_pos(caption, 16, 10);
  lv_obj_set_style_text_letter_space(caption, 2, 0);

  wifi.network_roller = lv_roller_create(panel);
  lv_obj_set_pos(wifi.network_roller, 16, 36);
  lv_obj_set_size(wifi.network_roller, 424, 140);
  lv_roller_set_options(wifi.network_roller, "Сканирование...",
                        LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(wifi.network_roller, 3);
  style_roller(wifi.network_roller);

  wifi.status_label = make_label(panel, "", &theme().muted14);
  lv_obj_set_pos(wifi.status_label, 16, 184);
  lv_obj_set_size(wifi.status_label, 424, 22);
  lv_label_set_long_mode(wifi.status_label, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* refresh = lv_button_create(panel);
  lv_obj_set_pos(refresh, 16, 218);
  lv_obj_set_size(refresh, 200, 44);
  make_ghost_button(refresh);
  lv_obj_add_event_cb(refresh, refresh_cb, LV_EVENT_CLICKED, &wifi);
  lv_obj_t* refresh_label = make_label(refresh, "Обновить", &theme().text18);
  lv_obj_set_size(refresh_label, 190, 24);
  lv_obj_set_style_text_align(refresh_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(refresh_label);

  wifi.connect_button = lv_button_create(panel);
  lv_obj_set_pos(wifi.connect_button, 240, 218);
  lv_obj_set_size(wifi.connect_button, 200, 44);
  make_primary_button(wifi.connect_button);
  lv_obj_add_event_cb(wifi.connect_button, connect_cb, LV_EVENT_CLICKED, &wifi);
  make_primary_label(wifi.connect_button, "Подключиться");

  wifi.footer = make_label(scr, "Сети с * требуют пароль", &theme().muted14);
  lv_obj_set_pos(wifi.footer, 12, 406);
  lv_obj_set_size(wifi.footer, 456, 20);
  lv_obj_set_style_text_align(wifi.footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(wifi.footer, LV_LABEL_LONG_MODE_DOTS);
}

void wifi_hide_password(WifiScreen& wifi) {
  close_password(static_cast<PasswordModal*>(wifi.password_modal));
}

void wifi_update(WifiScreen& wifi, const AppState& state) {
  status_bar_update(wifi.status, state);

  char current[64] = {};
  if (state.wifi_ssid[0] != '\0') {
    std::snprintf(current, sizeof(current), "Сейчас: %s", state.wifi_ssid);
  } else {
    std::snprintf(current, sizeof(current), "Сейчас: не задана");
  }
  set_label_text_safe(wifi.current_label, current);

  if (state.wifi_status[0] != '\0') {
    set_label_text_safe(wifi.status_label, state.wifi_status);
  }
  if (state.wifi_scan.scanning) {
    set_label_text(wifi.status_label, "Сканирование...");
    lv_obj_add_state(wifi.connect_button, LV_STATE_DISABLED);
    return;
  }
  if (state.wifi_scan.generation != wifi.last_generation) {
    rebuild_networks(wifi, state);
    if (state.wifi_scan.count == 0) {
      set_label_text(wifi.status_label, "Сети не найдены");
    } else {
      char status[48] = {};
      std::snprintf(status, sizeof(status), "Найдено: %u",
                    static_cast<unsigned>(state.wifi_scan.count));
      set_label_text(wifi.status_label, status);
    }
  }
}

}  // namespace ui
}  // namespace tablo
