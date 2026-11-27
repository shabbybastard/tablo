#include "net_worker.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include <cstring>
#include <ctime>

#include "backlight.h"
#include "core/date_format.h"
#include "core/miio.h"
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
constexpr uint32_t kExternalInterval = 5UL * 60UL * 1000UL;

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

void NetWorker::request_update_check() {
  request_update_.store(true, std::memory_order_release);
}
void NetWorker::request_install_update() {
  request_install_.store(true, std::memory_order_release);
}
void NetWorker::request_weather_refresh() {
  request_weather_.store(true, std::memory_order_release);
}
void NetWorker::request_timezone_apply() {
  request_timezone_.store(true, std::memory_order_release);
}
void NetWorker::request_wifi_scan() {
  request_scan_.store(true, std::memory_order_release);
}
void NetWorker::request_external_refresh() {
  request_external_.store(true, std::memory_order_release);
}
void NetWorker::reset_external_session() {
  // Called from the UI task: only publish a flag. MiioClient is owned by the
  // net task and must not be touched from another core.
  reset_external_.store(true, std::memory_order_release);
  request_external_.store(true, std::memory_order_release);
}

void NetWorker::update_clock(bool force) {
  const uint32_t now_ms = millis();
  if (!force && now_ms - last_clock_ms_ < 1000) return;
  last_clock_ms_ = now_ms;

  const time_t now = time(nullptr);
  const Settings settings = settings_.get();
  if (now < 1700000000 || now > 2100000000) {
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
  format_date_text(time.date_text, sizeof(time.date_text), local.tm_mday,
                   local.tm_mon + 1, local.tm_year + 1900, local.tm_wday,
                   settings.date_format);

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

void NetWorker::sync_external_config(const Settings& settings) {
  ExternalInfo info = g_state.snapshot().external;
  info.enabled = settings.purifier_enabled;
  std::snprintf(info.ip, sizeof(info.ip), "%s", settings.purifier_ip);
  info.token_set = settings.purifier_token[0] != '\0';

  uint8_t token[kMiioTokenBytes] = {};
  const bool token_ok = miio_parse_token(settings.purifier_token, token);
  const bool ip_ok = miio_valid_ipv4(settings.purifier_ip);
  info.configured = ip_ok && token_ok;

  if (!info.enabled) {
    info.valid = false;
    info.online = false;
    std::snprintf(info.status, sizeof(info.status), "Выключено");
  } else if (!info.configured) {
    info.valid = false;
    info.online = false;
    std::snprintf(info.status, sizeof(info.status), "Не настроено");
  } else if (!info.valid) {
    std::snprintf(info.status, sizeof(info.status), "Ожидание опроса");
  }
  g_state.update_external(info);
}

void NetWorker::poll_external(const Settings& settings) {
  const uint32_t now_ms = millis();
  ExternalInfo info = g_state.snapshot().external;
  info.enabled = settings.purifier_enabled;
  std::snprintf(info.ip, sizeof(info.ip), "%s", settings.purifier_ip);
  info.token_set = settings.purifier_token[0] != '\0';

  uint8_t token[kMiioTokenBytes] = {};
  const bool token_ok = miio_parse_token(settings.purifier_token, token);
  const bool ip_ok = miio_valid_ipv4(settings.purifier_ip);
  info.configured = ip_ok && token_ok;

  if (!info.enabled) {
    info.valid = false;
    info.online = false;
    std::snprintf(info.status, sizeof(info.status), "Выключено");
    external_fail_count_ = 0;
    next_external_ms_ = now_ms + kExternalInterval;
    g_state.update_external(info);
    return;
  }
  if (!info.configured) {
    info.valid = false;
    info.online = false;
    std::snprintf(info.status, sizeof(info.status), "Проверьте IP и токен");
    external_fail_count_ = 0;
    next_external_ms_ = now_ms + kExternalInterval;
    g_state.update_external(info);
    LOGW("miio: not configured (ip=%s token=%s)", ip_ok ? "ok" : "bad",
         token_ok ? "set" : "missing");
    return;
  }

  MiioReading reading;
  char model[sizeof(external_model_)] = {};
  char error[48] = {};
  const bool ok =
      miio_.poll(settings.purifier_ip, token, external_model_, reading, model,
                 sizeof(model), error, sizeof(error));
  if (model[0] != '\0') {
    std::snprintf(info.model, sizeof(info.model), "%s", model);
  }

  if (ok) {
    external_fail_count_ = 0;
    next_external_ms_ = now_ms + kExternalInterval;
    std::snprintf(external_model_, sizeof(external_model_), "%s", model);

    info.valid = true;
    info.online = true;
    info.power = reading.power;
    info.power_valid = reading.has_power;
    info.mode = reading.mode;
    info.fan_level = reading.fan_level;
    info.pm25 = reading.pm25;
    info.temperature = reading.temperature;
    info.temperature_valid = reading.has_temperature;
    info.humidity = reading.humidity;
    info.humidity_valid = reading.has_humidity;
    info.filter_life = reading.filter_life;
    info.tvoc = reading.tvoc;
    info.updated_uptime_s = now_ms / 1000U;
    std::snprintf(info.model, sizeof(info.model), "%s", model);
    std::snprintf(info.status, sizeof(info.status), "Подключено");
    g_state.update_external(info);
    LOGI("miio: model=%s pm25=%d temp=%d hum=%d filter=%d mode=%d fan=%d",
         model, static_cast<int>(reading.pm25),
         reading.has_temperature ? static_cast<int>(reading.temperature) : -1,
         reading.has_humidity ? static_cast<int>(reading.humidity) : -1,
         static_cast<int>(reading.filter_life), static_cast<int>(reading.mode),
         static_cast<int>(reading.fan_level));
    return;
  }

  static const uint32_t kRetryMs[] = {30000, 60000, 120000, 300000, 300000};
  if (external_fail_count_ < 4) ++external_fail_count_;
  const uint8_t index = external_fail_count_ > 0 ? external_fail_count_ - 1 : 0;
  next_external_ms_ = now_ms + kRetryMs[index > 4 ? 4 : index];

  info.online = false;
  if (!info.valid) {
    info.power_valid = false;
    info.mode = -1;
    info.fan_level = -1;
    info.pm25 = -1;
    info.temperature_valid = false;
    info.humidity_valid = false;
    info.filter_life = -1;
    info.tvoc = -1;
  }
  std::snprintf(info.status, sizeof(info.status), "%s",
                error[0] != '\0' ? error : "Нет связи");
  g_state.update_external(info);
  LOGW("miio: poll failed: %s (next in %u s)",
       error[0] != '\0' ? error : "no response",
       static_cast<unsigned>((next_external_ms_ - now_ms) / 1000U));
}

void NetWorker::fetch_weather(const Settings& settings) {
  if (!settings.weather_enabled) {
    weather_fail_count_ = 0;
    next_weather_ms_ = millis() + kWeatherInterval;
    const AppState state = g_state.snapshot();
    if (state.weather.valid || state.weather.status[0] == '\0') {
      WeatherInfo disabled;
      snprintf(disabled.status, sizeof(disabled.status), "Погода выключена");
      g_state.update_weather(disabled);
    }
    return;
  }
  if (settings.weather_lat[0] == '\0' || settings.weather_lon[0] == '\0') {
    weather_fail_count_ = 0;
    next_weather_ms_ = millis() + kWeatherInterval;
    WeatherInfo empty;
    snprintf(empty.status, sizeof(empty.status), "Не заданы координаты");
    g_state.update_weather(empty);
    LOGW("weather: coordinates are not set, skipped");
    return;
  }

  static const uint32_t kRetryMs[] = {0, 60000, 300000, 900000, 900000};
  auto schedule_retry = [this](const char* status) {
    if (weather_fail_count_ < 4) ++weather_fail_count_;
    const uint8_t index = weather_fail_count_ > 4 ? 4 : weather_fail_count_;
    next_weather_ms_ = millis() + kRetryMs[index];
    const AppState state = g_state.snapshot();
    if (!state.weather.valid) {
      WeatherInfo failed;
      snprintf(failed.status, sizeof(failed.status), "%s", status);
      g_state.update_weather(failed);
    }
  };

  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += settings.weather_lat;
  url += "&longitude=";
  url += settings.weather_lon;
  url += "&current=temperature_2m,apparent_temperature,weather_code&timezone=auto";

  HttpResponse response;
  WeatherInfo weather;
  if (!http_get(url, 4096, response) || response.status != 200) {
    schedule_retry("Нет связи");
    LOGW("weather: request failed (%d), next in %u s", response.status,
         static_cast<unsigned>((next_weather_ms_ - millis()) / 1000U));
    return;
  }
  if (!parse_openmeteo_weather(response.body.c_str(), response.body.length(), weather)) {
    schedule_retry("Ошибка ответа");
    LOGW("weather: parse failed (%u bytes)", static_cast<unsigned>(response.body.length()));
    return;
  }

  String air = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=";
  air += settings.weather_lat;
  air += "&longitude=";
  air += settings.weather_lon;
  air += "&current=pm2_5,us_aqi";
  HttpResponse air_response;
  if (http_get(air, 4096, air_response) && air_response.status == 200) {
    parse_openmeteo_air(air_response.body.c_str(), air_response.body.length(), weather);
  } else {
    LOGW("weather: air request failed (%d)", air_response.status);
  }

  weather.status[0] = '\0';
  weather.valid = true;
  weather_fail_count_ = 0;
  next_weather_ms_ = millis() + kWeatherInterval;
  g_state.update_weather(weather);
  LOGI("weather: %.0f C, %s, AQI %d", static_cast<double>(weather.temp_c), weather.condition,
       weather.aqi);
}

void NetWorker::run_wifi_scan() {
  WifiScanInfo scan;
  const AppState before = g_state.snapshot();
  scan.generation = before.wifi_scan.generation + 1;
  scan.scanning = true;
  g_state.update_wifi_scan(scan);

  const int found = WiFi.scanNetworks(false, true);
  scan.scanning = false;
  if (found > 0) {
    for (int i = 0; i < found && scan.count < kMaxWifiScan; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;
      WifiScanEntry& entry = scan.entries[scan.count++];
      snprintf(entry.ssid, sizeof(entry.ssid), "%s", ssid.c_str());
      entry.rssi = static_cast<int8_t>(WiFi.RSSI(i));
      entry.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    // Strongest networks first; the roller is easier to use that way.
    for (uint8_t i = 1; i < scan.count; ++i) {
      WifiScanEntry item = scan.entries[i];
      int j = i - 1;
      while (j >= 0 && scan.entries[j].rssi < item.rssi) {
        scan.entries[j + 1] = scan.entries[j];
        --j;
      }
      scan.entries[j + 1] = item;
    }
  }
  WiFi.scanDelete();
  g_state.update_wifi_scan(scan);
  LOGI("wifi: scan found %d networks", found);
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
  OtaInfo ota = g_state.snapshot().ota;
  ManifestInfo manifest;
  if (ota_check_manifest(settings, manifest, ota)) {
    manifest_ = manifest;
    manifest_valid_ = true;
    ota_fail_count_ = 0;
    next_ota_ms_ = millis() + kOtaInterval;
  } else {
    manifest_valid_ = false;
    if (ota_fail_count_ < 4) ++ota_fail_count_;
    static const uint32_t kRetryMs[] = {0, 60000, 300000, 900000, 3600000};
    const uint8_t index = ota_fail_count_ > 4 ? 4 : ota_fail_count_;
    next_ota_ms_ = millis() + kRetryMs[index];
    LOGW("ota: next check in %lu s",
         static_cast<unsigned long>(kRetryMs[index] / 1000U));
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
  sync_external_config(settings_.get());
  while (true) {
    web_.loop();
    wifi_.loop();
    update_wifi_state();
    update_clock(false);

    // Consume UI requests before taking the settings snapshot: the platform
    // saves settings before setting the flag, so the snapshot sees the new
    // timezone/weather switch.
    const bool timezone_requested =
        request_timezone_.exchange(false, std::memory_order_acq_rel);
    const bool scan_requested =
        request_scan_.exchange(false, std::memory_order_acq_rel);
    const Settings settings = settings_.get();
    if (timezone_requested) {
      ntp_started_ = false;
      next_ntp_ms_ = millis();
    }
    if (scan_requested) {
      run_wifi_scan();
    }
    const bool connected = wifi_.connected();
    if (connected != wifi_was_connected_) {
      wifi_was_connected_ = connected;
      if (connected) {
        ntp_started_ = false;
        next_ntp_ms_ = 0;
        miio_.reset();
        external_model_[0] = '\0';
        external_fail_count_ = 0;
        next_external_ms_ = millis() + 10000;
        LOGI("net: wifi ip=%s gw=%s dns=%s rssi=%d",
             WiFi.localIP().toString().c_str(),
             WiFi.gatewayIP().toString().c_str(),
             WiFi.dnsIP().toString().c_str(), WiFi.RSSI());
      } else {
        LOGW("net: wifi down");
      }
    }
    if (connected) {
      const uint32_t now_ms = millis();
      if (!ntp_started_) {
        configTzTime(settings.timezone, "pool.ntp.org", "time.cloudflare.com",
                     "162.159.200.1");
        ntp_started_ = true;
        next_ntp_ms_ = now_ms + 60000;
        LOGI("ntp: request tz='%s'", settings.timezone);
      }
      const time_t now = time(nullptr);
      if (now > 1700000000 && now < 2100000000) {
        if (!time_valid_) {
          time_valid_ = true;
          LOGI("ntp: time synced (epoch %lld)", static_cast<long long>(now));
        }
      } else if (now_ms >= next_ntp_ms_) {
        const sntp_sync_status_t status = sntp_get_sync_status();
        if (status == SNTP_SYNC_STATUS_RESET) {
          configTzTime(settings.timezone, "pool.ntp.org", "time.cloudflare.com",
                       "162.159.200.1");
          LOGI("ntp: restart, status=%d", static_cast<int>(status));
          next_ntp_ms_ = now_ms + 300000;
        } else {
          next_ntp_ms_ = now_ms + 30000;
        }
      }
      if (!time_valid_ && now_ms - last_ntp_log_ms_ >= 30000U) {
        last_ntp_log_ms_ = now_ms;
        LOGI("ntp: waiting, status=%d",
             static_cast<int>(sntp_get_sync_status()));
      }
      if (now_ms - last_heap_log_ms_ >= 300000U) {
        last_heap_log_ms_ = now_ms;
        LOGI("heap: free=%u max=%u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
      }

      if (!time_valid_ &&
          request_update_.exchange(false, std::memory_order_acq_rel)) {
        OtaInfo ota = g_state.snapshot().ota;
        ota.checked = true;
        ota.error = true;
        snprintf(ota.status, sizeof(ota.status), "Нет времени для HTTPS");
        g_state.update_ota(ota);
      }
      if (time_valid_) {
        if (!boot_ota_checked_) {
          if (settings.auto_ota && now_ms > 15000) {
            boot_ota_checked_ = true;
            check_ota(settings, true);
          }
        } else if (settings.auto_ota && now_ms >= next_ota_ms_) {
          check_ota(settings, false);
        }
        if (request_update_.exchange(false, std::memory_order_acq_rel)) {
          check_ota(settings, true);
        }
        if (request_install_.exchange(false, std::memory_order_acq_rel)) {
          maybe_install_ota(settings);
        }
        if (request_weather_.exchange(false, std::memory_order_acq_rel) ||
            now_ms >= next_weather_ms_) {
          // Re-read at fetch time: the weather switch may have changed after
          // the loop snapshot.
          const Settings weather_settings = settings_.get();
          fetch_weather(weather_settings);
        }
      }
      if (!qemu_detected() &&
          (request_external_.exchange(false, std::memory_order_acq_rel) ||
           now_ms >= next_external_ms_)) {
        if (reset_external_.exchange(false, std::memory_order_acq_rel)) {
          miio_.reset();
          external_model_[0] = '\0';
        }
        // Re-read at poll time: IP, token and the enable switch can change
        // while the loop is between iterations.
        poll_external(settings_.get());
      }
      if (millis() - last_ping_ms_ > kPingInterval) update_pings(settings);
    }
    delay(10);
  }
}

}  // namespace tablo
