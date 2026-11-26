#pragma once

#include <atomic>
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
  void request_weather_refresh();
  void request_timezone_apply();
  void request_wifi_scan();

 private:
  static void task_entry(void* arg);
  void run();

  void update_clock(bool force);
  void update_wifi_state();
  void fetch_weather(const Settings& settings);
  void run_wifi_scan();
  void update_pings(const Settings& settings);
  void check_ota(const Settings& settings, bool force);
  void maybe_install_ota(const Settings& settings);

  NvsSettings& settings_;
  WifiManager& wifi_;
  WebServerService web_;

  std::atomic<bool> request_update_{false};
  std::atomic<bool> request_install_{false};
  std::atomic<bool> request_weather_{false};
  std::atomic<bool> request_timezone_{false};
  std::atomic<bool> request_scan_{false};

  ManifestInfo manifest_{};
  bool manifest_valid_ = false;

  uint32_t last_clock_ms_ = 0;
  uint32_t last_wifi_ms_ = 0;
  uint32_t last_ping_ms_ = 0;
  uint32_t next_weather_ms_ = 0;
  uint32_t next_ota_ms_ = 0;
  uint32_t next_ntp_ms_ = 0;
  uint32_t last_ntp_log_ms_ = 0;
  uint32_t last_heap_log_ms_ = 0;
  uint8_t weather_fail_count_ = 0;
  uint8_t ota_fail_count_ = 0;
  bool ntp_started_ = false;
  bool wifi_was_connected_ = false;
  bool time_valid_ = false;
  bool boot_ota_checked_ = false;
};

}  // namespace tablo
