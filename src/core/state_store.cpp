#include "state_store.h"

#include <cstddef>
#include <cstring>

namespace tablo {
namespace {

void copy_text(char* dst, size_t cap, const char* src) {
  if (dst == nullptr || cap == 0) {
    return;
  }
  if (src == nullptr) {
    src = "";
  }
  const size_t len = std::strlen(src);
  const size_t n = (len < cap - 1) ? len : (cap - 1);
  std::memcpy(dst, src, n);
  dst[n] = '\0';
}

}  // namespace

StateStore g_state;

void StateStore::set(const AppState& state) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = state;
  state_.revision = state.revision + 1;
}

AppState StateStore::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

void StateStore::update_wifi(bool connected, int8_t rssi, const char* ip) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.wifi_connected = connected;
  state_.wifi_rssi = rssi;
  copy_text(state_.ip, sizeof(state_.ip), ip);
  ++state_.revision;
}

void StateStore::update_setup(const SetupInfo& setup) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.setup = setup;
  ++state_.revision;
}

void StateStore::update_sd_import(const SdImportInfo& import) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.sd_import = import;
  ++state_.revision;
}

void StateStore::update_weather(const WeatherInfo& weather) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.weather = weather;
  ++state_.revision;
}

void StateStore::update_pings(const PingInfo* pings) {
  if (pings == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  for (size_t i = 0; i < kMaxPingTargets; ++i) {
    state_.pings[i] = pings[i];
  }
  ++state_.revision;
}

void StateStore::update_ota(const OtaInfo& ota) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.ota = ota;
  ++state_.revision;
}

DisplayMode StateStore::update_clock_and_display(
    const TimeInfo& time, DisplayMode mode, bool night_mode,
    const NightSchedule& schedule) {
  std::lock_guard<std::mutex> lock(mutex_);
  const DisplayMode previous = state_.display_mode;
  state_.time = time;
  state_.display_mode = mode;
  state_.night_mode = night_mode;
  state_.schedule = schedule;
  ++state_.revision;
  return previous;
}

DisplayMode StateStore::update_display(DisplayMode mode, bool night_mode,
                                        const NightSchedule& schedule) {
  std::lock_guard<std::mutex> lock(mutex_);
  const DisplayMode previous = state_.display_mode;
  state_.display_mode = mode;
  state_.night_mode = night_mode;
  state_.schedule = schedule;
  ++state_.revision;
  return previous;
}

void StateStore::update_night_mode(bool night_mode, DisplayMode mode) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.night_mode = night_mode;
  state_.display_mode = mode;
  ++state_.revision;
}

void StateStore::update_brightness(uint8_t brightness) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.brightness = brightness;
  ++state_.revision;
}

}  // namespace tablo
