#include "net_worker.h"

#include <Arduino.h>
#include <ctime>

#include "backlight.h"
#include "core/state_store.h"
#include "core/version.h"
#include "http_client.h"
#include "log_ring.h"
#include "ota_github.h"
#include "ping.h"
#include "qemu.h"

namespace tablo {
namespace {
constexpr uint32_t kWeatherInterval = 15UL * 60UL * 1000UL;
constexpr uint32_t kPingInterval = 30UL * 1000UL;
constexpr uint32_t kOtaInterval = 6UL * 60UL * 60UL * 1000UL;

const char* const kMonths[] = {"янв", "фев", "мар", "апр", "мая", "июн",
                               "июл", "авг", "сен", "окт", "ноя", "дек"};
const char* const kWeekdays[] = {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
}  // namespace

NetWorker::NetWorker(NvsSettings& settings, WifiManager& wifi)
    : settings_(settings), wifi_(wifi), web_(settings) {}

void NetWorker::begin() {
  web_.begin(false);
  const BaseType_t created =
      xTaskCreatePinnedToCore(task_entry, "net", 16384, this, 1, nullptr, 0);
  if (created != pdPASS) {
    LOGE("net: worker task creation failed");
    return;
  }
  LOGI("net: worker started on core 0%s",
       qemu_detected() ? " (QEMU offline mode)" : "");
}

void NetWorker::task_entry(void* arg) {
  static_cast<NetWorker*>(arg)->run();
}

void NetWorker::request_update_check() { request_update_ = true; }
void NetWorker::request_install_update() { request_install_ = true; }

void NetWorker::update_clock(bool force) {
  const uint32_t now_ms = millis();
  if (!force && now_ms - last_clock_ms_ < 1000) return;
  last_clock_ms_ = now_ms;

  const time_t now = time(nullptr);
  const Settings settings = settings_.get();
  if (now < 1700000000) {
    const AppState state = g_state.snapshot();
    if (settings.schedule.enabled &&
        (state.display_mode != DisplayMode::Day || state.night_mode)) {
      const DisplayMode mode = display_mode_for(settings.schedule, 0, false);
      const DisplayMode previous =
          g_state.update_display(mode, mode == DisplayMode::Night,
                                 settings.schedule);
      if (mode != previous) {
        apply_current_backlight();
      }
    }
    return;
  }

  struct tm local = {};
  localtime_r(&now, &local);

  TimeInfo time;
  time.valid = true;
  time.hour = static_cast<uint8_t>(local.tm_hour);
  time.minute = static_cast<uint8_t>(local.tm_min);
  snprintf(time.date_text, sizeof(time.date_text), "%d %s · %s", local.tm_mday,
           kMonths[local.tm_mon % 12], kWeekdays[local.tm_wday % 7]);

  const uint16_t minute =
      static_cast<uint16_t>(time.hour) * 60U + time.minute;
  const DisplayMode mode =
      settings.schedule.enabled
          ? display_mode_for(settings.schedule, minute, true)
          : (settings.night_mode ? DisplayMode::Night : DisplayMode::Day);
  const DisplayMode previous = g_state.update_clock_and_display(
      time, mode, mode == DisplayMode::Night, settings.schedule);

  if (mode != previous) {
    apply_current_backlight();
    LOGI("schedule: mode=%u (day=%u evening=%u night=%u)",
         static_cast<unsigned>(mode),
         static_cast<unsigned>(settings.schedule.day_min),
         static_cast<unsigned>(settings.schedule.evening_min),
         static_cast<unsigned>(settings.schedule.night_min));
  }
}

void NetWorker::update_wifi_state() {
  if (millis() - last_wifi_ms_ < 5000) return;
  last_wifi_ms_ = millis();
  const bool connected = wifi_.connected();
  const int rssi = wifi_.rssi();
  const String ip = wifi_.ip();
  g_state.update_wifi(connected, static_cast<int8_t>(rssi), ip.c_str());
}

void NetWorker::fetch_weather(const Settings& settings) {
  if (settings.weather_lat[0] == '\0' || settings.weather_lon[0] == '\0') return;
  last_weather_ms_ = millis();

  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += settings.weather_lat;
  url += "&longitude=";
  url += settings.weather_lon;
  url += "&current=temperature_2m,apparent_temperature,weather_code&timezone=auto";

  HttpResponse response;
  WeatherInfo weather;
  if (!http_get(url, 4096, response) || response.status != 200) {
    LOGW("weather: request failed (%d)", response.status);
    return;
  }
  if (!parse_openmeteo_weather(response.body.c_str(), response.body.length(), weather)) {
    LOGW("weather: parse failed");
    return;
  }

  String air = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=";
  air += settings.weather_lat;
  air += "&longitude=";
  air += settings.weather_lon;
  air += "&current=pm2_5,us_aqi";
  HttpResponse air_response;
  if (http_get(air, 4096, air_response) &&
      air_response.status == 200) {
    parse_openmeteo_air(air_response.body.c_str(), air_response.body.length(), weather);
  }

  weather.valid = true;
  g_state.update_weather(weather);
  LOGI("weather: %.0f C, %s, AQI %d", static_cast<double>(weather.temp_c), weather.condition,
       weather.aqi);
}

void NetWorker::update_pings(const Settings& settings) {
  last_ping_ms_ = millis();
  PingInfo pings[kMaxPingTargets];
  const char* hosts[kMaxPingTargets] = {settings.ping_gw, settings.ping_vps, settings.ping_dns};
  const char* labels[kMaxPingTargets] = {"GW", "VPS", "DNS"};
  for (size_t i = 0; i < kMaxPingTargets; ++i) {
    snprintf(pings[i].label, sizeof(pings[i].label), "%s", labels[i]);
    pings[i].configured = hosts[i][0] != '\0';
    pings[i].online = false;
    pings[i].ms = 0;
    if (pings[i].configured) {
      uint16_t ms = 0;
      if (ping_host(hosts[i], ms)) {
        pings[i].online = true;
        pings[i].ms = ms;
      }
    }
  }
  g_state.update_pings(pings);
}

void NetWorker::check_ota(const Settings& settings, bool force) {
  if (!time_valid_ && !force) return;
  last_ota_ms_ = millis();
  OtaInfo ota = g_state.snapshot().ota;
  ManifestInfo manifest;
  if (ota_check_manifest(settings, manifest, ota)) {
    manifest_ = manifest;
    manifest_valid_ = true;
  } else {
    manifest_valid_ = false;
  }
  g_state.update_ota(ota);
}

void NetWorker::maybe_install_ota(const Settings& settings) {
  if (!manifest_valid_ || !g_state.snapshot().ota.available) {
    check_ota(settings, true);
    if (!manifest_valid_ || !g_state.snapshot().ota.available) return;
  }
  OtaInfo ota = g_state.snapshot().ota;
  if (!ota_install(settings, manifest_, ota)) {
    ota.downloading = false;
    g_state.update_ota(ota);
  }
}

void NetWorker::run() {
  update_wifi_state();
  update_clock(true);
  while (true) {
    web_.loop();
    wifi_.loop();
    update_wifi_state();
    update_clock(false);

    const Settings settings = settings_.get();
    if (wifi_.connected()) {
      if (!ntp_started_) {
        configTzTime(settings.timezone, "pool.ntp.org", "time.cloudflare.com");
        ntp_started_ = true;
        LOGI("ntp: requested timezone %s", settings.timezone);
      }
      if (time(nullptr) > 1700000000) time_valid_ = true;

      if (time_valid_) {
        if (!boot_ota_checked_ && settings.auto_ota && millis() > 15000) {
          boot_ota_checked_ = true;
          check_ota(settings, true);
        }
        if (settings.auto_ota && millis() - last_ota_ms_ > kOtaInterval) {
          check_ota(settings, false);
        }
        if (request_update_) {
          request_update_ = false;
          check_ota(settings, true);
        }
        if (request_install_) {
          request_install_ = false;
          maybe_install_ota(settings);
        }
        if (millis() - last_weather_ms_ > kWeatherInterval) fetch_weather(settings);
        if (millis() - last_ping_ms_ > kPingInterval) update_pings(settings);
      }
    }
    delay(10);
  }
}

}  // namespace tablo
