#include "wifi_manager.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <esp_random.h>
#include <cstring>

#include "core/settings_validate.h"
#include "display.h"
#include "log_ring.h"
#include "qemu.h"

namespace tablo {
namespace {
String html_escape(const String& in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }
  return out;
}

String portal_page(const Settings& settings, const String& error = "") {
  String page;
  page.reserve(3200);
  page += F("<!doctype html><html lang=ru><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>tablo setup</title><style>"
            "body{margin:0;background:#0b0f14;color:#e7edf5;font:16px system-ui,sans-serif;"
            "display:flex;min-height:100vh;align-items:center;justify-content:center}"
            "main{width:min(440px,92vw);background:#111820;border:1px solid #24313d;"
            "border-radius:18px;padding:28px}h1{font-size:24px;margin:0 0 8px}"
            "p{color:#91a0b0;line-height:1.5}label{display:block;margin:18px 0 6px;color:#b8c5d3}"
            "input{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;"
            "border:1px solid #2d3b49;background:#0b1118;color:#e7edf5;font-size:16px}"
            "button{margin-top:22px;width:100%;padding:13px;border:0;border-radius:10px;"
            "background:#4fc3f7;color:#021018;font-weight:700;font-size:16px}"
            ".err{color:#ff8a80;margin-top:14px}small{color:#637283}</style></head><body><main>"
            "<h1>tablo</h1><p>Подключите дисплей к Wi-Fi и задайте пароль администратора. "
            "После сохранения устройство перезагрузится.</p>");
  if (error.length()) {
    page += "<div class=err>" + html_escape(error) + "</div>";
  }
  page += "<form method=post action=/save>";
  page += "<label>SSID</label><input name=ssid maxlength=32 autocomplete=off required value=\"" +
          html_escape(settings.wifi_ssid) + "\">";
  page += "<label>Пароль Wi-Fi</label><input name=pass type=password maxlength=64 "
          "placeholder='оставьте пустым, чтобы не менять'>";
  page += "<label>Admin user</label><input name=admin_user maxlength=31 required value=\"" +
          html_escape(settings.admin_user) + "\">";
  page += "<label>Admin password (мин. 8 символов)</label>"
          "<input name=admin_pass type=password maxlength=63 required>";
  page += "<button type=submit>Сохранить и перезагрузить</button></form>";
  page += F("<p><small>Пароль администратора нужен для /config, /update и /logs. "
            "Он хранится только в NVS устройства. Открытая точка доступа нужна только "
            "для настройки.</small></p></main></body></html>");
  return page;
}

bool needs_admin_setup(const Settings& settings) {
  return settings.admin_user[0] == '\0' || settings.admin_password[0] == '\0' ||
         std::strcmp(settings.admin_password, "change-me") == 0;
}
}  // namespace

WifiManager::WifiManager(NvsSettings& settings) : settings_(settings) {
  uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac());
  snprintf(ap_ssid_, sizeof(ap_ssid_), "tablo-%04X", static_cast<unsigned>(chip & 0xFFFF));
  uint8_t random_bytes[16] = {};
  esp_fill_random(random_bytes, sizeof(random_bytes));
  ap_password_[0] = 't';
  for (size_t i = 0; i < sizeof(random_bytes); ++i) {
    snprintf(ap_password_ + 1 + i * 2, 3, "%02X", random_bytes[i]);
  }
}

void WifiManager::begin(bool allow_portal, bool portal_on_failure) {
  if (qemu_detected()) {
    // Espressif QEMU does not model the Wi-Fi PHY. The blocking first-run
    // SoftAP portal would never return, so keep the rest of the firmware
    // (LVGL, web server, net worker) running in offline mode instead.
    offline_ = true;
    LOGW("wifi: QEMU detected, Wi-Fi is unavailable, continuing offline");
    return;
  }

  const Settings settings = settings_.get();
  if (settings_need_setup(settings)) {
    if (allow_portal) {
      start_portal();
    } else {
      LOGW("wifi: setup required, rescue mode cannot connect");
    }
    return;
  }

  begin_station();
  for (int i = 0; i < 200 && !connected(); ++i) {
    delay(100);
    if (i > 0 && (i % 20) == 0) {
      LOGI("wifi: connecting to %s...", settings.wifi_ssid);
    }
  }
  if (connected()) {
    LOGI("wifi: connected, ip=%s rssi=%d", ip().c_str(), rssi());
    if (!mdns_started_ && MDNS.begin("tablo")) {
      MDNS.addService("http", "tcp", 80);
      mdns_started_ = true;
      LOGI("wifi: mDNS http://tablo.local");
    }
  } else {
    LOGW("wifi: connection failed, will retry in background");
    if (portal_on_failure && allow_portal) {
      LOGW("wifi: rescue fallback to setup portal");
      start_portal();
    }
  }
}

void WifiManager::begin_station() {
  const Settings settings = settings_.get();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.wifi_ssid, settings.wifi_password);
  last_retry_ms_ = millis();
}

void WifiManager::loop() {
  if (offline_) {
    return;
  }
  if (connected()) {
    if (!mdns_started_ && MDNS.begin("tablo")) {
      MDNS.addService("http", "tcp", 80);
      mdns_started_ = true;
    }
    return;
  }
  if (millis() - last_retry_ms_ > 30000) {
    last_retry_ms_ = millis();
    LOGW("wifi: reconnecting...");
    WiFi.reconnect();
  }
}

void WifiManager::start_portal() {
  LOGI("wifi: starting setup portal %s (WPA2, password %s)", ap_ssid_, ap_password_);
  char hint[64];
  snprintf(hint, sizeof(hint), "pass: %s", ap_password_);
  display_show_message("Wi-Fi setup", ap_ssid_, hint);

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(300);
  WiFi.mode(WIFI_AP);
  delay(300);
  if (!WiFi.softAP(ap_ssid_, ap_password_, 6, 0, 4)) {
    LOGE("wifi: softAP failed");
    return;
  }
  delay(200);
  dns_.start(53, "*", WiFi.softAPIP());
  portal_.on("/", HTTP_GET, [this]() { handle_root(); });
  portal_.on("/save", HTTP_POST, [this]() { handle_save(); });
  portal_.onNotFound([this]() {
    portal_.sendHeader("Location", "http://192.168.4.1/");
    portal_.send(302, "text/plain", "");
  });
  portal_.begin();
  portal_active_ = true;
  LOGI("wifi: portal ready at %s", WiFi.softAPIP().toString().c_str());

  while (portal_active_) {
    dns_.processNextRequest();
    portal_.handleClient();
    delay(2);
  }
}

void WifiManager::handle_root() {
  portal_.send(200, "text/html; charset=utf-8", portal_page(settings_.get()));
}

void WifiManager::handle_save() {
  Settings settings = settings_.get();
  String ssid = portal_.arg("ssid");
  String pass = portal_.arg("pass");
  String admin_user = portal_.arg("admin_user");
  String admin_pass = portal_.arg("admin_pass");
  ssid.trim();
  admin_user.trim();

  if (ssid.length() == 0) {
    portal_.send(400, "text/html; charset=utf-8",
                 portal_page(settings, "Укажите SSID"));
    return;
  }
  if (pass.length() == 0 && std::strcmp(ssid.c_str(), settings.wifi_ssid) != 0) {
    portal_.send(400, "text/html; charset=utf-8",
                 portal_page(settings, "Для новой сети нужен пароль Wi-Fi"));
    return;
  }
  if (admin_user.length() == 0) {
    portal_.send(400, "text/html; charset=utf-8",
                 portal_page(settings, "Укажите admin user"));
    return;
  }
  if (admin_pass.length() == 0 && needs_admin_setup(settings)) {
    portal_.send(400, "text/html; charset=utf-8",
                 portal_page(settings, "Задайте пароль администратора (мин. 8 символов)"));
    return;
  }
  if (admin_pass.length() > 0 && admin_pass.length() < 8) {
    portal_.send(400, "text/html; charset=utf-8",
                 portal_page(settings, "Пароль администратора — минимум 8 символов"));
    return;
  }

  std::strncpy(settings.wifi_ssid, ssid.c_str(), sizeof(settings.wifi_ssid) - 1);
  settings.wifi_ssid[sizeof(settings.wifi_ssid) - 1] = '\0';
  if (pass.length() > 0) {
    std::strncpy(settings.wifi_password, pass.c_str(), sizeof(settings.wifi_password) - 1);
    settings.wifi_password[sizeof(settings.wifi_password) - 1] = '\0';
  }
  std::strncpy(settings.admin_user, admin_user.c_str(), sizeof(settings.admin_user) - 1);
  settings.admin_user[sizeof(settings.admin_user) - 1] = '\0';
  if (admin_pass.length() > 0) {
    std::strncpy(settings.admin_password, admin_pass.c_str(),
                 sizeof(settings.admin_password) - 1);
    settings.admin_password[sizeof(settings.admin_password) - 1] = '\0';
  }
  if (needs_admin_setup(settings)) {
    portal_.send(400, "text/html; charset=utf-8",
                 portal_page(settings, "Слабый пароль администратора"));
    return;
  }
  if (!settings_.save_checked(settings)) {
    portal_.send(500, "text/html; charset=utf-8",
                 portal_page(settings, "Не удалось сохранить настройки в NVS"));
    return;
  }
  LOGI("wifi: setup saved for %s, rebooting", settings.wifi_ssid);
  portal_.send(200, "text/html; charset=utf-8",
               "<!doctype html><meta charset=utf-8><body style='background:#0b0f14;color:#e7edf5;"
               "font:18px system-ui;padding:40px'>Сохранено. Перезагрузка…</body>");
  delay(900);
  ESP.restart();
}

bool WifiManager::connected() const {
  return !offline_ && WiFi.status() == WL_CONNECTED;
}

String WifiManager::ip() const {
  return connected() ? WiFi.localIP().toString() : String("0.0.0.0");
}

int WifiManager::rssi() const {
  return connected() ? WiFi.RSSI() : 0;
}

}  // namespace tablo
