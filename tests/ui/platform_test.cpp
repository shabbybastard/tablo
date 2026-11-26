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
