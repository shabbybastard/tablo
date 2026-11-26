#include "platform_esp32.h"

#include <Arduino.h>

#include <atomic>

#include "backlight.h"
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
