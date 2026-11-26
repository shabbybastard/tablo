#include "dashboard.h"

#include <cstdio>

#include "theme.h"
#include "widgets.h"

namespace tablo {
namespace ui {
namespace {

constexpr int kCardW = 222;
constexpr int kCardH = 206;
constexpr int kCardX1 = 12;
constexpr int kCardX2 = 246;
constexpr int kCardY1 = 48;
constexpr int kCardY2 = 266;

void set_long_label(lv_obj_t* label, int32_t width, int32_t height,
                    lv_label_long_mode_t mode,
                    lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_set_size(label, width, height);
  lv_label_set_long_mode(label, mode);
  lv_obj_set_style_text_align(label, align, 0);
}

const char* weather_symbol(int16_t code) {
  if (code < 0) return "";
  if (code == 0 || code == 1 || code == 2) return "\xE2\x98\x80";  // ☀
  if (code == 3) return "\xE2\x98\x81";                            // ☁
  if (code == 45 || code == 48) return "\xE2\x89\x8B";             // ≋ fog
  if (code >= 51 && code <= 57) return "\xE2\x98\x82";             // ☂ drizzle
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) {
    return "\xE2\x98\x94";                                         // ☔ rain
  }
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
    return "\xE2\x9D\x84";                                         // ❄ snow
  }
  if (code >= 95) return "\xE2\x9A\xA1";                           // ⚡ storm
  return "\xE2\x98\x81";
}

void format_ip(const AppState& state, char* out, size_t out_size) {
  if (state.ip[0] != '\0') {
    std::snprintf(out, out_size, "IP %s", state.ip);
  } else {
    std::snprintf(out, out_size, "IP -");
  }
}

}  // namespace

void dashboard_create(Dashboard& d, lv_event_cb_t version_cb,
                      lv_event_cb_t gesture_cb, lv_event_cb_t menu_cb) {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  remove_flags(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  if (gesture_cb != nullptr) {
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, nullptr);
  }
  d.screen = scr;

  status_bar_create(d.status, scr, "⚙", menu_cb, version_cb);

  /* ---------------------------------------------------------------- clock */
  lv_obj_t* clock_card = make_card(scr, kCardX1, kCardY1, kCardW, kCardH);

  lv_obj_t* clock_caption = make_label(clock_card, "СЕЙЧАС", &theme().muted14);
  lv_obj_set_pos(clock_caption, 14, 10);
  lv_obj_set_style_text_letter_space(clock_caption, 2, 0);

  d.clock_label = make_label(clock_card, "--:--", &theme().clock48);
  lv_obj_set_pos(d.clock_label, 14, 30);
  set_long_label(d.clock_label, 194, 62, LV_LABEL_LONG_MODE_CLIP,
                 LV_TEXT_ALIGN_CENTER);

  d.date_label = make_label(clock_card, "Время не получено",
                            &theme().text18);
  lv_obj_set_pos(d.date_label, 14, 94);
  set_long_label(d.date_label, 194, 26, LV_LABEL_LONG_MODE_DOTS,
                 LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_text_color(d.date_label, palette().muted, 0);

  make_divider(clock_card, 14, 126, 194);

  d.wifi_icon_card = make_dot(clock_card, 14, 146, 8, palette().muted);

  d.wifi_label = make_label(clock_card, "Нет сети", &theme().muted14);
  lv_obj_set_pos(d.wifi_label, 28, 140);
  set_long_label(d.wifi_label, 180, 20, LV_LABEL_LONG_MODE_DOTS);

  d.ota_label = make_label(clock_card, "OTA не проверялась", &theme().text14);
  lv_obj_set_pos(d.ota_label, 14, 166);
  set_long_label(d.ota_label, 194, 20, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_style_text_color(d.ota_label, palette().muted, 0);

  /* -------------------------------------------------------------- weather */
  lv_obj_t* weather_card = make_card(scr, kCardX2, kCardY1, kCardW, kCardH);

  lv_obj_t* weather_caption = make_label(weather_card, "ПОГОДА", &theme().muted14);
  lv_obj_set_pos(weather_caption, 14, 10);
  lv_obj_set_style_text_letter_space(weather_caption, 2, 0);

  d.weather_temp = make_label(weather_card, "--°", &theme().text28);
  lv_obj_set_pos(d.weather_temp, 14, 31);
  set_long_label(d.weather_temp, 80, 38, LV_LABEL_LONG_MODE_CLIP);

  d.weather_icon = make_label(weather_card, "", &theme().text18);
  lv_obj_set_pos(d.weather_icon, 96, 43);
  set_long_label(d.weather_icon, 24, 24, LV_LABEL_LONG_MODE_CLIP);

  d.weather_condition = make_label(weather_card, "Нет данных", &theme().text18);
  lv_obj_set_pos(d.weather_condition, 124, 43);
  set_long_label(d.weather_condition, 84, 24, LV_LABEL_LONG_MODE_DOTS);

  d.weather_feels = make_label(weather_card, "Ощущается как --°",
                                &theme().muted14);
  lv_obj_set_pos(d.weather_feels, 14, 72);
  set_long_label(d.weather_feels, 194, 20, LV_LABEL_LONG_MODE_DOTS);

  make_divider(weather_card, 14, 98, 194);

  d.aqi_dot = make_dot(weather_card, 14, 116, 10, palette().muted);
  d.aqi_label = make_label(weather_card, "AQI --", &theme().text18);
  lv_obj_set_pos(d.aqi_label, 32, 108);
  set_long_label(d.aqi_label, 100, 24, LV_LABEL_LONG_MODE_CLIP);

  d.pm_label = make_label(weather_card, "PM2.5 -- мкг/м3", &theme().muted14);
  lv_obj_set_pos(d.pm_label, 14, 138);
  set_long_label(d.pm_label, 194, 20, LV_LABEL_LONG_MODE_DOTS);

  d.aqi_quality = make_label(weather_card, "Воздух: --",
                              &theme().text14);
  lv_obj_set_pos(d.aqi_quality, 14, 164);
  set_long_label(d.aqi_quality, 194, 20, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_style_text_color(d.aqi_quality, palette().muted, 0);

  /* ---------------------------------------------------------------- net */
  lv_obj_t* net_card = make_card(scr, kCardX1, kCardY2, kCardW, kCardH);

  lv_obj_t* net_caption = make_label(net_card, "СЕТЬ", &theme().muted14);
  lv_obj_set_pos(net_caption, 14, 10);
  lv_obj_set_style_text_letter_space(net_caption, 2, 0);

  d.net_ip = make_label(net_card, "IP -", &theme().text18);
  lv_obj_set_pos(d.net_ip, 14, 34);
  set_long_label(d.net_ip, 194, 24, LV_LABEL_LONG_MODE_DOTS);

  make_divider(net_card, 14, 64, 194);

  static const char* const kDefaultPings[3] = {"GW", "VPS", "DNS"};
  for (int i = 0; i < 3; ++i) {
    const int32_t y = 78 + i * 38;
    d.ping_pill[i] = make_dot(net_card, 16, y + 6, 9, palette().muted);

    d.ping_name[i] = make_label(net_card, kDefaultPings[i], &theme().muted14);
    lv_obj_set_pos(d.ping_name[i], 34, y);
    set_long_label(d.ping_name[i], 64, 20, LV_LABEL_LONG_MODE_DOTS);

    d.ping_label[i] = make_label(net_card, "-", &theme().accent14);
    lv_obj_set_pos(d.ping_label[i], 100, y - 1);
    set_long_label(d.ping_label[i], 108, 22, LV_LABEL_LONG_MODE_CLIP,
                   LV_TEXT_ALIGN_RIGHT);
  }

  /* --------------------------------------------------------------- note */
  lv_obj_t* note_card = make_card(scr, kCardX2, kCardY2, kCardW, kCardH);

  lv_obj_t* note_caption = make_label(note_card, "ЗАМЕТКА", &theme().muted14);
  lv_obj_set_pos(note_caption, 14, 10);
  lv_obj_set_style_text_letter_space(note_caption, 2, 0);

  d.note_label = make_label(note_card, "Нет заметки", &theme().text14);
  lv_obj_set_pos(d.note_label, 14, 34);
  set_long_label(d.note_label, 194, 88, LV_LABEL_LONG_MODE_DOTS);

  make_divider(note_card, 14, 126, 194);

  lv_obj_t* uptime_caption = make_label(note_card, "Время работы",
                                        &theme().muted14);
  lv_obj_set_pos(uptime_caption, 14, 136);
  set_long_label(uptime_caption, 104, 20, LV_LABEL_LONG_MODE_CLIP);

  d.uptime_label = make_label(note_card, "-", &theme().accent14);
  lv_obj_set_pos(d.uptime_label, 118, 136);
  set_long_label(d.uptime_label, 90, 20, LV_LABEL_LONG_MODE_DOTS,
                 LV_TEXT_ALIGN_RIGHT);

  d.note_ip = make_label(note_card, "IP -", &theme().muted14);
  lv_obj_set_pos(d.note_ip, 14, 166);
  set_long_label(d.note_ip, 194, 20, LV_LABEL_LONG_MODE_DOTS);
}

void dashboard_update(Dashboard& d, const AppState& state) {
  status_bar_update(d.status, state);
  const Palette& p = palette();
  const bool mode_changed = d.last_mode != theme().mode;
  if (mode_changed) {
    lv_obj_set_style_text_color(d.date_label, p.muted, 0);
  }

  /* Clock + date. */
  char buffer[96];
  if (state.time.valid) {
    std::snprintf(buffer, sizeof(buffer), "%02u:%02u",
                  static_cast<unsigned>(state.time.hour),
                  static_cast<unsigned>(state.time.minute));
    set_label_text(d.clock_label, buffer);
    if (state.time.date_text[0] != '\0') {
      set_label_text_safe(d.date_label, state.time.date_text);
    } else {
      set_label_text(d.date_label, "Дата не получена");
    }
  } else {
    set_label_text(d.clock_label, "--:--");
    set_label_text(d.date_label, "Время не получено");
  }

  /* Clock card: Wi-Fi + OTA. */
  if (state.wifi_connected) {
    if (state.wifi_rssi != 0) {
      std::snprintf(buffer, sizeof(buffer), "Wi-Fi %d dBm",
                    static_cast<int>(state.wifi_rssi));
    } else {
      std::snprintf(buffer, sizeof(buffer), "Wi-Fi");
    }
  } else {
    std::snprintf(buffer, sizeof(buffer), "Нет сети");
  }
  set_label_text(d.wifi_label, buffer);

  if (mode_changed || state.wifi_connected != d.last_wifi) {
    lv_obj_set_style_bg_color(d.wifi_icon_card,
                              state.wifi_connected ? p.green : p.muted, 0);
    lv_obj_set_style_text_color(d.wifi_label,
                                state.wifi_connected ? p.text : p.muted, 0);
    d.last_wifi = state.wifi_connected;
  }

  int ota_kind = 0;
  if (state.ota.downloading) {
    ota_kind = 3;
    std::snprintf(buffer, sizeof(buffer), "Загрузка %d%%",
                  static_cast<int>(state.ota.progress));
  } else if (state.ota.available) {
    ota_kind = 2;
    if (state.ota.version[0] != '\0') {
      std::snprintf(buffer, sizeof(buffer), "Обновление %s",
                    state.ota.version);
    } else {
      std::snprintf(buffer, sizeof(buffer), "Доступно обновление");
    }
  } else if (state.ota.checked) {
    ota_kind = state.ota.error ? 4 : 1;
    if (state.ota.status[0] != '\0') {
      std::snprintf(buffer, sizeof(buffer), "%s", state.ota.status);
    } else {
      std::snprintf(buffer, sizeof(buffer), "Обновлений нет");
    }
  } else {
    std::snprintf(buffer, sizeof(buffer), "OTA не проверялась");
  }
  set_label_text_safe(d.ota_label, buffer);
  if (mode_changed || ota_kind != d.last_ota_kind) {
    lv_color_t color = p.muted;
    if (ota_kind == 3 || ota_kind == 2) {
      color = p.accent;
    } else if (ota_kind == 1) {
      color = p.green;
    } else if (ota_kind == 4) {
      color = p.red;
    }
    lv_obj_set_style_text_color(d.ota_label, color, 0);
    d.last_ota_kind = ota_kind;
  }

  /* Weather + air quality. */
  const bool weather_valid = state.weather.valid;
  if (weather_valid) {
    std::snprintf(buffer, sizeof(buffer), "%d°",
                  static_cast<int>(state.weather.temp_c));
    set_label_text(d.weather_temp, buffer);
    set_label_text_safe(d.weather_icon,
                        weather_symbol(state.weather.weather_code));
    set_label_text_safe(d.weather_condition,
                        state.weather.condition[0] != '\0'
                            ? state.weather.condition
                            : "Без данных");
    std::snprintf(buffer, sizeof(buffer), "Ощущается как %d°",
                  static_cast<int>(state.weather.feels_like_c));
    set_label_text(d.weather_feels, buffer);

    std::snprintf(buffer, sizeof(buffer), "AQI %d",
                  static_cast<int>(state.weather.aqi));
    set_label_text(d.aqi_label, buffer);
    std::snprintf(buffer, sizeof(buffer), "PM2.5 %d мкг/м3",
                  static_cast<int>(state.weather.pm25));
    set_label_text(d.pm_label, buffer);
    std::snprintf(buffer, sizeof(buffer), "Воздух: %s",
                  aqi_text(state.weather.aqi));
    set_label_text(d.aqi_quality, buffer);
  } else {
    set_label_text(d.weather_temp, "--°");
    set_label_text(d.weather_icon, "");
    set_label_text_safe(d.weather_condition,
                        state.weather.status[0] != '\0'
                            ? state.weather.status
                            : "Нет данных");
    set_label_text(d.weather_feels, "Ощущается как --°");
    set_label_text(d.aqi_label, "AQI --");
    set_label_text(d.pm_label, "PM2.5 -- мкг/м3");
    set_label_text(d.aqi_quality, "Воздух: нет данных");
  }

  if (mode_changed || weather_valid != d.last_weather_valid ||
      state.weather.aqi != d.last_aqi) {
    const lv_color_t air = weather_valid ? aqi_color(state.weather.aqi)
                                         : p.muted;
    lv_obj_set_style_bg_color(d.aqi_dot, air, 0);
    lv_obj_set_style_text_color(d.aqi_quality, air, 0);
    d.last_weather_valid = weather_valid;
    d.last_aqi = state.weather.aqi;
  }

  /* Network card: IP + ping rows. */
  format_ip(state, buffer, sizeof(buffer));
  set_label_text_safe(d.net_ip, buffer);

  static const char* const kDefaultNames[3] = {"GW", "VPS", "DNS"};
  for (int i = 0; i < 3; ++i) {
    const PingInfo& ping = state.pings[i];
    const char* name = ping.label[0] != '\0' ? ping.label : kDefaultNames[i];
    set_label_text_safe(d.ping_name[i], name);

    int ping_state = 0;
    if (ping.configured) {
      ping_state = ping.online ? 1 : 2;
    }

    if (ping_state == 1) {
      std::snprintf(buffer, sizeof(buffer), "%u мс",
                    static_cast<unsigned>(ping.ms));
    } else if (ping_state == 2) {
      std::snprintf(buffer, sizeof(buffer), "--");
    } else {
      std::snprintf(buffer, sizeof(buffer), "-");
    }
    set_label_text(d.ping_label[i], buffer);

    if (mode_changed || ping_state != d.last_ping_state[i]) {
      lv_color_t color = p.muted;
      if (ping_state == 1) {
        color = p.green;
      } else if (ping_state == 2) {
        color = p.red;
      }
      lv_obj_set_style_bg_color(d.ping_pill[i], color, 0);
      d.last_ping_state[i] = ping_state;
    }
  }

  /* Note card: note + uptime + IP. */
  set_label_text_safe(d.note_label,
                      state.note[0] != '\0' ? state.note : "Нет заметки");

  format_uptime(state.uptime_s, buffer, sizeof(buffer));
  set_label_text(d.uptime_label, buffer);

  format_ip(state, buffer, sizeof(buffer));
  set_label_text_safe(d.note_ip, buffer);

  d.last_mode = theme().mode;
}

}  // namespace ui
}  // namespace tablo
