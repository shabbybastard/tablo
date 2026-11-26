#include "core/platform.h"

#include <cstdio>

#include "core/state_store.h"

namespace tablo {
namespace {
int g_reboots = 0;
int g_last_action = -1;

class TestPlatform final : public IPlatform {
 public:
  void setBrightness(uint8_t percent) override {
    AppState state = g_state.snapshot();
    state.brightness = percent;
    g_state.set(state);
  }

  void setNightMode(bool on) override {
    AppState state = g_state.snapshot();
    state.night_mode = on;
    state.display_mode = on ? DisplayMode::Night : DisplayMode::Day;
    g_state.set(state);
  }

  void setNightSchedule(const NightSchedule& schedule) override {
    AppState state = g_state.snapshot();
    state.schedule = schedule;
    g_state.set(state);
  }

  void setTimezone(const char* timezone) override {
    AppState state = g_state.snapshot();
    std::snprintf(state.timezone, sizeof(state.timezone), "%s", timezone);
    g_state.update_options(state.date_format, state.weather_enabled,
                           state.timezone);
  }

  void setDateFormat(DateFormat format) override {
    AppState state = g_state.snapshot();
    g_state.update_options(format, state.weather_enabled, state.timezone);
  }

  void setWeatherEnabled(bool enabled) override {
    AppState state = g_state.snapshot();
    g_state.update_options(state.date_format, enabled, state.timezone);
    if (!enabled) {
      WeatherInfo weather;
      std::snprintf(weather.status, sizeof(weather.status), "Погода выключена");
      g_state.update_weather(weather);
    }
  }

  void requestWifiScan() override {
    AppState state = g_state.snapshot();
    WifiScanInfo scan;
    scan.generation = state.wifi_scan.generation + 1;
    scan.count = 1;
    std::snprintf(scan.entries[0].ssid, sizeof(scan.entries[0].ssid), "TestNet");
    scan.entries[0].rssi = -50;
    scan.entries[0].secure = true;
    g_state.update_wifi_scan(scan);
  }

  void connectWifi(const char*, const char*) override {}

  void checkForUpdates() override {
    AppState state = g_state.snapshot();
    state.ota.checked = true;
    state.ota.available = true;
    state.ota.downloading = false;
    std::snprintf(state.ota.status, sizeof(state.ota.status),
                  "Доступно обновление");
    g_state.set(state);
  }

  void installUpdate() override {
    AppState state = g_state.snapshot();
    state.ota.downloading = true;
    state.ota.progress = 0;
    std::snprintf(state.ota.status, sizeof(state.ota.status),
                  "Загрузка обновления");
    g_state.set(state);
  }

  void reboot() override {
    ++g_reboots;
    std::printf("[ui-test] platform::reboot() requested\n");
  }

  void requestAction(PlatformAction action) override {
    g_last_action = static_cast<int>(action);
  }
};
}  // namespace

IPlatform& platform() {
  static TestPlatform instance;
  return instance;
}

int test_platform_reboots() { return g_reboots; }

int test_platform_last_action() { return g_last_action; }

}  // namespace tablo
