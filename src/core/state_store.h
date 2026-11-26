#pragma once

#include <mutex>

#include "app_state.h"

namespace tablo {

/** Thread-safe snapshot holder shared by the network worker and the UI. */
class StateStore {
 public:
  StateStore() = default;

  void set(const AppState& state);
  AppState snapshot() const;
  void update_wifi(bool connected, int8_t rssi, const char* ip);
  void update_setup(const SetupInfo& setup);
  void update_sd_import(const SdImportInfo& import);

  /* Field-level updates keep the network worker from overwriting a mode or
   * brightness change made by the UI in the same tick. */
  void update_weather(const WeatherInfo& weather);
  void update_pings(const PingInfo* pings);
  void update_ota(const OtaInfo& ota);
  DisplayMode update_clock_and_display(const TimeInfo& time, DisplayMode mode,
                                       bool night_mode,
                                       const NightSchedule& schedule);
  DisplayMode update_display(DisplayMode mode, bool night_mode,
                             const NightSchedule& schedule);
  void update_night_mode(bool night_mode, DisplayMode mode);
  void update_brightness(uint8_t brightness);
  void update_options(DateFormat date_format, bool weather_enabled,
                      const char* timezone);
  void update_wifi_scan(const WifiScanInfo& scan);
  void update_wifi_status(const char* status);
  void update_wifi_identity(const char* ssid, const char* status);

 private:
  mutable std::mutex mutex_;
  AppState state_;
};

extern StateStore g_state;

}  // namespace tablo
