#include "dashboard.h"

#include <cstdio>

#include "core/miio.h"
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

/** 0 unknown/disabled, 1 clean, 2 normal, 3 moderate, 4 bad. */
int purifier_band(int pm25) {
  if (pm25 < 0) return 0;
  if (pm25 <= 15) return 1;
  if (pm25 <= 35) return 2;
  if (pm25 <= 55) return 3;
  return 4;
}

lv_color_t purifier_color(int band) {
  if (band == 1) return palette().green;
  if (band == 2) return palette().accent;
  if (band == 3) return palette().amber;
  if (band == 4) return palette().red;
  return palette().muted;
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

  status_bar_create(d.status, scr, "⚙", menu_cb, version_cb, true);

  /* ---------------------------------------------------------------- clock */
  lv_obj_t* clock_card = make_card(scr, kCardX1, kCardY1, 456, kCardH);

  lv_obj_t* clock_caption = make_label(clock_card, "СЕЙЧАС", &theme().muted14);
  lv_obj_set_pos(clock_caption, 14, 10);
  lv_obj_set_style_text_letter_space(clock_caption, 2, 0);

  d.clock_label = make_label(clock_card, "--:--", &theme().clock48);
  lv_obj_set_pos(d.clock_label, 14, 50);
  set_long_label(d.clock_label, 428, 62, LV_LABEL_LONG_MODE_CLIP,
                 LV_TEXT_ALIGN_CENTER);

  d.date_label = make_label(clock_card, "Время не получено",
                            &theme().text18);
  lv_obj_set_pos(d.date_label, 14, 118);
  set_long_label(d.date_label, 428, 26, LV_LABEL_LONG_MODE_DOTS,
                 LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_text_color(d.date_label, palette().muted, 0);

  make_divider(clock_card, 14, 146, 428);

  d.note_label = make_label(clock_card, "Нет заметки", &theme().muted14);
  lv_obj_set_pos(d.note_label, 14, 156);
  set_long_label(d.note_label, 428, 40, LV_LABEL_LONG_MODE_WRAP,
                 LV_TEXT_ALIGN_CENTER);

  /* -------------------------------------------------------------- weather */
  lv_obj_t* weather_card = make_card(scr, kCardX1, kCardY2, kCardW, kCardH);

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

  /* ------------------------------------------------------------ purifier */
  lv_obj_t* purifier_card = make_card(scr, kCardX2, kCardY2, kCardW, kCardH);

  lv_obj_t* purifier_caption = make_label(purifier_card, "ОЧИСТИТЕЛЬ",
                                           &theme().muted14);
  lv_obj_set_pos(purifier_caption, 14, 10);
  lv_obj_set_style_text_letter_space(purifier_caption, 2, 0);

  d.purifier_dot = make_dot(purifier_card, 196, 15, 10, palette().muted);

  d.purifier_pm = make_label(purifier_card, "--", &theme().text28);
  lv_obj_set_pos(d.purifier_pm, 14, 31);
  set_long_label(d.purifier_pm, 80, 38, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* purifier_metric = make_label(purifier_card, "PM2.5",
                                          &theme().muted14);
  lv_obj_set_pos(purifier_metric, 96, 31);
  lv_obj_set_size(purifier_metric, 100, 18);

  d.purifier_unit = make_label(purifier_card, "мкг/м3", &theme().muted14);
  lv_obj_set_pos(d.purifier_unit, 96, 51);
  set_long_label(d.purifier_unit, 100, 20, LV_LABEL_LONG_MODE_DOTS);

  d.purifier_detail = make_label(purifier_card, "--° · --%", &theme().text18);
  lv_obj_set_pos(d.purifier_detail, 14, 76);
  set_long_label(d.purifier_detail, 194, 24, LV_LABEL_LONG_MODE_DOTS);

  make_divider(purifier_card, 14, 104, 194);

  d.purifier_filter = make_label(purifier_card, "Фильтр --", &theme().text18);
  lv_obj_set_pos(d.purifier_filter, 14, 114);
  set_long_label(d.purifier_filter, 194, 24, LV_LABEL_LONG_MODE_DOTS);

  d.purifier_mode = make_label(purifier_card, "-", &theme().muted14);
  lv_obj_set_pos(d.purifier_mode, 14, 144);
  set_long_label(d.purifier_mode, 194, 20, LV_LABEL_LONG_MODE_DOTS);

  d.purifier_status = make_label(purifier_card, "Не настроено",
                                  &theme().muted14);
  lv_obj_set_pos(d.purifier_status, 14, 170);
  set_long_label(d.purifier_status, 194, 20, LV_LABEL_LONG_MODE_DOTS);
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

  set_label_text_safe(d.note_label,
                      state.note[0] != '\0' ? state.note : "Нет заметки");

  /* ---------------------------------------------------------- purifier */
  const ExternalInfo& ext = state.external;
  const int band = ext.valid ? purifier_band(ext.pm25) : 0;
  if (ext.valid && ext.pm25 >= 0) {
    std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(ext.pm25));
    set_label_text(d.purifier_pm, buffer);
  } else {
    set_label_text(d.purifier_pm, "--");
  }

  if (ext.valid) {
    if (ext.temperature_valid && ext.humidity_valid) {
      std::snprintf(buffer, sizeof(buffer), "%d° · %d%%",
                    static_cast<int>(ext.temperature),
                    static_cast<int>(ext.humidity));
    } else if (ext.temperature_valid) {
      std::snprintf(buffer, sizeof(buffer), "%d°",
                    static_cast<int>(ext.temperature));
    } else if (ext.humidity_valid) {
      std::snprintf(buffer, sizeof(buffer), "%d%%",
                    static_cast<int>(ext.humidity));
    } else {
      std::snprintf(buffer, sizeof(buffer), "--° · --%%");
    }
    set_label_text(d.purifier_detail, buffer);

    if (ext.filter_life >= 0) {
      std::snprintf(buffer, sizeof(buffer), "Фильтр %d%%",
                    static_cast<int>(ext.filter_life));
    } else {
      std::snprintf(buffer, sizeof(buffer), "Фильтр --");
    }
    set_label_text(d.purifier_filter, buffer);

    if (ext.power_valid && !ext.power) {
      set_label_text(d.purifier_mode, "Выключен");
    } else if (ext.mode >= 0) {
      if (ext.fan_level >= 0) {
        std::snprintf(buffer, sizeof(buffer), "%s · %d",
                      miio_mode_text(ext.mode),
                      static_cast<int>(ext.fan_level));
      } else {
        std::snprintf(buffer, sizeof(buffer), "%s", miio_mode_text(ext.mode));
      }
      set_label_text(d.purifier_mode, buffer);
    } else {
      set_label_text(d.purifier_mode, "-");
    }

    uint32_t age = 0;
    if (state.uptime_s >= ext.updated_uptime_s) {
      age = state.uptime_s - ext.updated_uptime_s;
    }
    if (ext.online) {
      if (age < 60) {
        set_label_text(d.purifier_status, "Обновлено только что");
      } else {
        std::snprintf(buffer, sizeof(buffer), "%u мин назад",
                      static_cast<unsigned>(age / 60U));
        set_label_text(d.purifier_status, buffer);
      }
    } else if (age < 60) {
      set_label_text(d.purifier_status, "Нет связи");
    } else {
      std::snprintf(buffer, sizeof(buffer), "Нет связи · %u мин",
                    static_cast<unsigned>(age / 60U));
      set_label_text(d.purifier_status, buffer);
    }
  } else {
    set_label_text(d.purifier_detail, "--° · --%");
    set_label_text(d.purifier_filter, "Фильтр --");
    set_label_text(d.purifier_mode, "-");
    if (!ext.enabled) {
      set_label_text(d.purifier_status, "Выключено");
    } else if (!ext.configured) {
      set_label_text(d.purifier_status, "Не настроено");
    } else if (ext.status[0] != '\0') {
      set_label_text_safe(d.purifier_status, ext.status);
    } else {
      set_label_text(d.purifier_status, "Ожидание опроса");
    }
  }

  if (mode_changed || band != d.last_purifier_band ||
      ext.online != d.last_purifier_online ||
      ext.enabled != d.last_purifier_enabled ||
      ext.configured != d.last_purifier_configured) {
    lv_obj_set_style_text_color(d.purifier_pm, purifier_color(band), 0);
    lv_color_t dot = p.muted;
    if (ext.enabled && ext.configured) {
      dot = ext.online ? p.green : (ext.valid ? p.amber : p.red);
    }
    lv_obj_set_style_bg_color(d.purifier_dot, dot, 0);
    d.last_purifier_band = band;
    d.last_purifier_online = ext.online;
    d.last_purifier_enabled = ext.enabled;
    d.last_purifier_configured = ext.configured;
  }

  d.last_mode = theme().mode;
}

}  // namespace ui
}  // namespace tablo
