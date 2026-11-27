#include "core/platform.h"

#include <emscripten.h>

#include <cstdio>

#include "core/app_state.h"
#include "core/state_store.h"
#include "hal_wasm/mock_provider.h"

namespace {

EM_JS(void, tablo_preview_set_brightness, (int percent), {
  if (typeof window !== 'undefined' && typeof window.tabloSetBrightness === 'function') {
    window.tabloSetBrightness(percent);
  }
});

EM_JS(void, tablo_preview_reload, (), {
  if (typeof location !== 'undefined') {
    location.reload();
  }
});

class WasmPlatform final : public tablo::IPlatform {
 public:
  void setBrightness(uint8_t percent) override {
    tablo::mock_provider().setBrightness(percent);

    tablo::AppState state = tablo::g_state.snapshot();
    state.brightness = percent;
    tablo::g_state.set(state);

    tablo_preview_set_brightness(percent);
  }

  void setNightMode(bool on) override {
    tablo::mock_provider().setNightMode(on);

    tablo::AppState state = tablo::g_state.snapshot();
    state.night_mode = on;
    state.display_mode = on ? tablo::DisplayMode::Night : tablo::DisplayMode::Day;
    tablo::g_state.set(state);
  }

  void setNightSchedule(const tablo::NightSchedule& schedule) override {
    tablo::mock_provider().setSchedule(schedule);
    tablo::AppState state = tablo::g_state.snapshot();
    state.schedule = schedule;
    tablo::g_state.set(state);
  }

  void setTimezone(const char* timezone) override {
    tablo::AppState state = tablo::g_state.snapshot();
    std::snprintf(state.timezone, sizeof(state.timezone), "%s", timezone);
    tablo::g_state.update_options(state.date_format, state.weather_enabled,
                                  state.timezone);
  }

  void setDateFormat(tablo::DateFormat format) override {
    tablo::AppState state = tablo::g_state.snapshot();
    tablo::g_state.update_options(format, state.weather_enabled,
                                  state.timezone);
  }

  void setWeatherEnabled(bool enabled) override {
    tablo::AppState state = tablo::g_state.snapshot();
    tablo::g_state.update_options(state.date_format, enabled, state.timezone);
    if (!enabled) {
      tablo::WeatherInfo weather;
      std::snprintf(weather.status, sizeof(weather.status), "Погода выключена");
      tablo::g_state.update_weather(weather);
    }
  }

  void setExternalConfig(bool enabled, const char* ip,
                         const char* token) override {
    tablo::mock_provider().setExternalConfig(enabled, ip, token);
  }

  void requestExternalRefresh() override {
    tablo::mock_provider().requestExternalRefresh();
  }

  void requestWifiScan() override {
    tablo::AppState state = tablo::g_state.snapshot();    tablo::WifiScanInfo scan;
    scan.generation = state.wifi_scan.generation + 1;
    scan.count = 3;
    std::snprintf(scan.entries[0].ssid, sizeof(scan.entries[0].ssid), "Home");
    scan.entries[0].rssi = -42;
    scan.entries[0].secure = true;
    std::snprintf(scan.entries[1].ssid, sizeof(scan.entries[1].ssid), "Guest");
    scan.entries[1].rssi = -61;
    scan.entries[1].secure = false;
    std::snprintf(scan.entries[2].ssid, sizeof(scan.entries[2].ssid), "Neighbors");
    scan.entries[2].rssi = -78;
    scan.entries[2].secure = true;
    tablo::g_state.update_wifi_scan(scan);
  }

  void connectWifi(const char*, const char*) override { reboot(); }

  void checkForUpdates() override {
    tablo::mock_provider().requestUpdateCheck();
  }

  void installUpdate() override {
    tablo::mock_provider().requestInstallUpdate();
  }

  void reboot() override {
    tablo_preview_reload();
  }

  void requestAction(tablo::PlatformAction) override {}
};

}  // namespace

namespace tablo {

IPlatform& platform() {
  static WasmPlatform instance;
  return instance;
}

}  // namespace tablo
