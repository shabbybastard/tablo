#include "platform_esp32.h"

#include <Arduino.h>

#include <atomic>

#include "backlight.h"
#include "core/miio.h"
#include "core/platform.h"
#include "core/state_store.h"
#include "log_ring.h"

namespace tablo {
namespace {
NvsSettings* g_settings = nullptr;
NetWorker* g_net = nullptr;
std::atomic<uint32_t> g_actions{0};

class Esp32Platform final : public IPlatform {
 public:
  void setBrightness(uint8_t percent) override {
    if (percent > 100) percent = 100;
    g_state.update_brightness(percent);
    apply_current_backlight();
    if (g_settings) g_settings->set_brightness(percent);
    LOGI("ui: brightness=%u%%", static_cast<unsigned>(percent));
  }

  void setNightMode(bool on) override {
    AppState state = g_state.snapshot();
    if (state.schedule.enabled) {
      LOGW("ui: manual night mode ignored while schedule is enabled");
      return;
    }
    state.night_mode = on;
    state.display_mode = on ? DisplayMode::Night : DisplayMode::Day;
    g_state.update_night_mode(on, state.display_mode);
    apply_current_backlight();
    if (g_settings) g_settings->set_night_mode(on);
    LOGI("ui: night mode=%s", on ? "on" : "off");
  }

  void setNightSchedule(const NightSchedule& schedule) override {
    if (!g_settings) return;
    g_settings->set_schedule(schedule);

    AppState state = g_state.snapshot();
    state.schedule = schedule;
    const Settings settings = g_settings->get();
    const uint16_t minute = state.time.valid
                                ? static_cast<uint16_t>(state.time.hour) * 60U +
                                      state.time.minute
                                : 0;
    const DisplayMode mode =
        schedule.enabled
            ? display_mode_for(schedule, minute, state.time.valid)
            : (settings.night_mode ? DisplayMode::Night : DisplayMode::Day);
    state.display_mode = mode;
    state.night_mode = mode == DisplayMode::Night;
    g_state.update_display(mode, state.night_mode, schedule);
    apply_current_backlight();
    LOGI("ui: schedule %s, evening=%u night=%u day=%u",
         schedule.enabled ? "enabled" : "disabled",
         static_cast<unsigned>(schedule.evening_min),
         static_cast<unsigned>(schedule.night_min),
         static_cast<unsigned>(schedule.day_min));
  }

  void setTimezone(const char* timezone) override {
    if (timezone == nullptr || timezone[0] == '\0') return;
    if (g_settings) g_settings->set_timezone(timezone);
    const Settings settings = g_settings ? g_settings->get() : Settings{};
    g_state.update_options(settings.date_format, settings.weather_enabled,
                           settings.timezone);
    if (g_net) g_net->request_timezone_apply();
    LOGI("ui: timezone=%s", timezone);
  }

  void setDateFormat(DateFormat format) override {
    if (g_settings) g_settings->set_date_format(format);
    const Settings settings = g_settings ? g_settings->get() : Settings{};
    g_state.update_options(settings.date_format, settings.weather_enabled,
                           settings.timezone);
    LOGI("ui: date format=%u", static_cast<unsigned>(format));
  }

  void setWeatherEnabled(bool enabled) override {
    if (g_settings) g_settings->set_weather_enabled(enabled);
    const Settings settings = g_settings ? g_settings->get() : Settings{};
    g_state.update_options(settings.date_format, settings.weather_enabled,
                           settings.timezone);
    if (!enabled) {
      WeatherInfo weather;
      snprintf(weather.status, sizeof(weather.status), "Погода выключена");
      g_state.update_weather(weather);
    } else if (g_net) {
      g_net->request_weather_refresh();
    }
    LOGI("ui: weather=%s", enabled ? "on" : "off");
  }

  void requestWifiScan() override {
    g_state.update_wifi_status("");
    if (g_net) g_net->request_wifi_scan();
  }

  void setExternalConfig(bool enabled, const char* ip,
                         const char* token) override {
    if (g_settings == nullptr) return;
    g_settings->set_external(enabled, ip, token);
    if (!g_settings->save_checked(g_settings->get())) {
      LOGW("ui: external settings not persisted");
    }
    const Settings settings = g_settings->get();

    ExternalInfo info = g_state.snapshot().external;
    info.enabled = settings.purifier_enabled;
    std::snprintf(info.ip, sizeof(info.ip), "%s", settings.purifier_ip);
    info.token_set = settings.purifier_token[0] != '\0';

    uint8_t token_bytes[kMiioTokenBytes] = {};
    const bool token_ok =
        miio_parse_token(settings.purifier_token, token_bytes);
    info.configured =
        miio_valid_ipv4(settings.purifier_ip) && token_ok;

    info.valid = false;
    info.online = false;
    info.power_valid = false;
    info.mode = -1;
    info.fan_level = -1;
    info.pm25 = -1;
    info.temperature_valid = false;
    info.humidity_valid = false;
    info.filter_life = -1;
    info.tvoc = -1;

    if (!info.enabled) {
      std::snprintf(info.status, sizeof(info.status), "Выключено");
    } else if (!info.configured) {
      std::snprintf(info.status, sizeof(info.status), "Не настроено");
    } else {
      std::snprintf(info.status, sizeof(info.status), "Ожидание опроса");
    }
    g_state.update_external(info);

    if (g_net != nullptr) {
      g_net->reset_external_session();
      g_net->request_external_refresh();
    }
    LOGI("ui: external enabled=%s ip=%s token=%s",
         info.enabled ? "on" : "off", info.ip,
         info.token_set ? "set" : "missing");
  }

  void requestExternalRefresh() override {
    if (g_net != nullptr) g_net->request_external_refresh();
  }

  void connectWifi(const char* ssid, const char* password) override {
    if (!g_settings || ssid == nullptr || ssid[0] == '\0') return;
    Settings settings = g_settings->get();
    snprintf(settings.wifi_ssid, sizeof(settings.wifi_ssid), "%s", ssid);
    snprintf(settings.wifi_password, sizeof(settings.wifi_password), "%s",
             password != nullptr ? password : "");
    if (!g_settings->save_checked(settings)) {
      g_state.update_wifi_status("Не удалось сохранить Wi-Fi");
      LOGE("ui: failed to save Wi-Fi credentials");
      return;
    }
    g_state.update_wifi_identity(ssid, "Сохранено, перезагрузка...");
    LOGI("ui: wifi credentials saved for '%s', rebooting", ssid);
    reboot();
  }

  void checkForUpdates() override {
    if (g_net) g_net->request_update_check();
    OtaInfo ota = g_state.snapshot().ota;
    snprintf(ota.status, sizeof(ota.status), "Проверка обновлений...");
    g_state.update_ota(ota);
  }

  void installUpdate() override {
    if (g_net) g_net->request_install_update();
  }

  void reboot() override {
    LOGI("ui: reboot requested");
    delay(150);
    ESP.restart();
  }

  void requestAction(PlatformAction action) override {
    const uint32_t bit = 1u << static_cast<uint32_t>(action);
    g_actions.fetch_or(bit, std::memory_order_release);
  }
};

Esp32Platform& instance() {
  static Esp32Platform p;
  return p;
}
}  // namespace

IPlatform& platform() { return instance(); }

void platform_bind(NvsSettings* settings, NetWorker* net) {
  g_settings = settings;
  g_net = net;
}

uint32_t platform_take_actions() {
  return g_actions.exchange(0, std::memory_order_acq_rel);
}

}  // namespace tablo
