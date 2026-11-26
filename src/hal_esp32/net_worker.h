#pragma once

#include <cstdint>

#include "core/parsers.h"
#include "settings_nvs.h"
#include "wifi_manager.h"
#include "web_server.h"

namespace tablo {

class NetWorker {
 public:
  NetWorker(NvsSettings& settings, WifiManager& wifi);
  void begin();
  void request_update_check();
  void request_install_update();

 private:
  static void task_entry(void* arg);
  void run();

  void update_clock(bool force);
  void update_wifi_state();
  void fetch_weather(const Settings& settings);
  void update_pings(const Settings& settings);
  void check_ota(const Settings& settings, bool force);
  void maybe_install_ota(const Settings& settings);

  NvsSettings& settings_;
  WifiManager& wifi_;
  WebServerService web_;

  volatile bool request_update_ = false;
  volatile bool request_install_ = false;

  ManifestInfo manifest_{};
  bool manifest_valid_ = false;

  uint32_t last_clock_ms_ = 0;
  uint32_t last_wifi_ms_ = 0;
  uint32_t last_weather_ms_ = 0;
  uint32_t last_ping_ms_ = 0;
  uint32_t last_ota_ms_ = 0;
  bool ntp_started_ = false;
  bool time_valid_ = false;
  bool boot_ota_checked_ = false;
};

}  // namespace tablo
