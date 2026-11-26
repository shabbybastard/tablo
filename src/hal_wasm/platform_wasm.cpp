#include "core/platform.h"

#include <emscripten.h>

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
