#pragma once

#include <WebServer.h>

#include "settings_nvs.h"

namespace tablo {

class WebServerService {
 public:
  explicit WebServerService(NvsSettings& settings);
  void begin(bool rescue_mode);
  void loop();

 private:
  void setup_routes(bool rescue_mode);
  bool authorized();
  void send_auth_required();
  String config_page(const String& message = "") const;
  String update_page(const String& message = "") const;
  void handle_root();
  void handle_config_get();
  void handle_config_post();
  void handle_update_get();
  void handle_logs();
  void handle_upload_start();
  void handle_upload_write();
  void handle_upload_done();

  NvsSettings& settings_;
  WebServer server_{80};
  bool started_ = false;
  bool rescue_mode_ = false;
  bool upload_allowed_ = false;
  bool upload_ok_ = false;
  bool upload_paused_ = false;
};

}  // namespace tablo
