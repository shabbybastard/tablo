#pragma once

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "settings_nvs.h"

namespace tablo {

class WifiManager {
 public:
  explicit WifiManager(NvsSettings& settings);
  ~WifiManager() = default;

  /** Connect, or block in the first-run SoftAP portal when no credentials exist. */
  void begin(bool allow_portal = true, bool portal_on_failure = false);
  void loop();

  bool connected() const;
  String ip() const;
  int rssi() const;
  const char* ap_name() const { return ap_ssid_; }
  const char* ap_password() const { return ap_password_; }

 private:
  void start_portal();
  void handle_root();
  void handle_save();
  void begin_station();

  NvsSettings& settings_;
  WebServer portal_{80};
  DNSServer dns_;
  char ap_ssid_[24] = "";
  char ap_password_[40] = "";
  uint32_t last_retry_ms_ = 0;
  bool mdns_started_ = false;
  bool portal_active_ = false;
  bool offline_ = false;
};

}  // namespace tablo
