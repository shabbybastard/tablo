#include "web_server.h"

#include <Arduino.h>
#include <Update.h>
#include <cstring>

#include "backlight.h"
#include "boot_guard.h"
#include "log_ring.h"
#include "qemu.h"
#include "render.h"

namespace tablo {
namespace {

String esc(const String& in) {
  String out;
  out.reserve(in.length() + 8);
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

void copy_arg(const String& value, char* dst, size_t cap, bool keep_empty = false) {
  if (keep_empty && value.length() == 0) return;
  std::strncpy(dst, value.c_str(), cap - 1);
  dst[cap - 1] = '\0';
}

void field(String& html, const char* label, const char* name, const char* value,
           const char* type = "text", const char* hint = nullptr) {
  html += "<label>" + String(label) + "</label><input type=" + type + " name=" + name +
          " value=\"" + esc(value ? value : "") + "\"";
  if (hint) html += String(" placeholder=\"") + hint + "\"";
  html += ">";
}

String page_head(const char* title) {
  String s;
  s.reserve(6000);
  s += "<!doctype html><html lang=ru><head><meta charset=utf-8>"
       "<meta name=viewport content='width=device-width,initial-scale=1'>"
       "<title>";
  s += title;
  s += "</title><style>"
       "body{margin:0;background:#0b0f14;color:#e7edf5;font:15px/1.5 system-ui,sans-serif}"
       "main{max-width:760px;margin:0 auto;padding:28px 18px 60px}"
       "h1{font-size:24px;margin:0 0 6px}h2{font-size:16px;margin:28px 0 10px;color:#8fd6ff}"
       "p{color:#91a0b0}.muted{color:#647486;font-size:13px}"
       "section{background:#111820;border:1px solid #24313d;border-radius:14px;padding:18px;margin:14px 0}"
       "label{display:block;margin:12px 0 5px;color:#b8c5d3;font-size:13px}"
       "input{width:100%;box-sizing:border-box;padding:10px;border-radius:9px;border:1px solid #2d3b49;"
       "background:#0b1118;color:#e7edf5;font-size:15px}"
       "input[type=checkbox]{width:auto;margin-right:8px}"
       ".row{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
       "button{margin-top:20px;padding:12px 20px;border:0;border-radius:9px;background:#4fc3f7;"
       "color:#021018;font-weight:700;font-size:15px;cursor:pointer}"
       ".btn{display:inline-block;margin:8px 8px 0 0;padding:10px 14px;border-radius:9px;"
       "background:#1b2835;color:#d9e6f2;text-decoration:none;border:1px solid #2d3b49}"
       ".ok{color:#7ee0a1}.err{color:#ff8a80}code{color:#8fd6ff}"
       "</style></head><body><main>";
  return s;
}

String page_foot(const char* extra = "") {
  String s = "<p class=muted>";
  s += extra;
  s += "</p></main></body></html>";
  return s;
}
}  // namespace

WebServerService::WebServerService(NvsSettings& settings) : settings_(settings) {}

void WebServerService::begin(bool rescue_mode) {
  rescue_mode_ = rescue_mode;
  if (qemu_detected()) {
    // QEMU has no Wi-Fi PHY and therefore no lwIP TCP/IP thread. Binding a
    // socket here asserts inside lwIP, so keep the service offline.
    LOGW("web: not started in QEMU (Wi-Fi/LWIP are not modeled)");
    return;
  }
  setup_routes(rescue_mode);
  server_.begin();
  started_ = true;
  LOGI("web: server started in %s mode", rescue_mode ? "RESCUE" : "normal");
}

void WebServerService::setup_routes(bool rescue_mode) {
  server_.on("/", HTTP_GET, [this]() { handle_root(); });
  server_.on("/update", HTTP_GET, [this]() { handle_update_get(); });
  server_.on("/update", HTTP_POST, [this]() { handle_upload_done(); },
             [this]() { handle_upload_write(); });
  server_.on("/logs", HTTP_GET, [this]() { handle_logs(); });
  if (!rescue_mode) {
    server_.on("/config", HTTP_GET, [this]() { handle_config_get(); });
    server_.on("/config", HTTP_POST, [this]() { handle_config_post(); });
  }
  server_.onNotFound([this]() {
    server_.sendHeader("Location", rescue_mode_ ? "/update" : "/config");
    server_.send(302, "text/plain", "");
  });
}

bool WebServerService::authorized() {
  const Settings s = settings_.get();
  return server_.authenticate(s.admin_user, s.admin_password);
}

void WebServerService::send_auth_required() {
  server_.requestAuthentication();
}

void WebServerService::loop() {
  if (started_) {
    server_.handleClient();
  }
}

void WebServerService::handle_root() {
  server_.sendHeader("Location", rescue_mode_ ? "/update" : "/config");
  server_.send(302, "text/plain", "");
}

String WebServerService::config_page(const String& message) const {
  const Settings s = settings_.get();
  String h = page_head("tablo config");
  h += "<h1>tablo</h1><p>Настройки хранятся в NVS и переживают OTA. "
       "Изменения применяются после перезагрузки.</p>";
  if (message.length()) h += "<p class=ok>" + esc(message) + "</p>";
  h += "<form method=post action=/config>";
  h += "<section><h2>Устройство</h2>";
  h += "<div class=row>";
  field(h, "Яркость, %", "bright", String(s.brightness).c_str(), "number");
  field(h, "Часовой пояс (POSIX TZ)", "tz", s.timezone);
  h += "</div>";
  field(h, "Заметка на экране", "note", s.note);
  h += "<label><input type=checkbox name=auto_ota ";
  if (s.auto_ota) h += "checked";
  h += "> Автоматически проверять OTA</label>";
  h += "<label>Формат даты <select name=date_format>";
  for (int i = 0; i < 3; ++i) {
    h += "<option value=";
    h += i;
    if (i == static_cast<int>(s.date_format)) h += " selected";
    h += ">";
    h += esc(date_format_label(static_cast<DateFormat>(i)));
    h += "</option>";
  }
  h += "</select></label></section>";

  char evening[8];
  char night[8];
  char day[8];
  format_hhmm(s.schedule.evening_min, evening, sizeof(evening));
  format_hhmm(s.schedule.night_min, night, sizeof(night));
  format_hhmm(s.schedule.day_min, day, sizeof(day));
  h += "<section><h2>Расписание экрана</h2>";
  h += "<label><input type=checkbox name=schedule_enabled ";
  if (s.schedule.enabled) h += "checked";
  h += "> Автоматически переключать вечер/ночь/день</label><div class=row>";
  field(h, "Вечер, HH:MM", "evening_start", evening);
  field(h, "Ночь, HH:MM", "night_start", night);
  field(h, "День, HH:MM", "day_start", day);
  h += "</div><p class=muted>День должен быть раньше вечера, вечер — раньше ночи. "
       "Вечер: 60% яркости, ночь: 25%.</p></section>";

  h += "<section><h2>Wi-Fi</h2><p class=muted>Оставьте пароль пустым, чтобы не менять.</p>";
  field(h, "SSID", "wifi_ssid", s.wifi_ssid);
  field(h, "Пароль", "wifi_pass", "", "password");
  field(h, "HTTP Basic user", "admin_user", s.admin_user);
  field(h, "HTTP Basic password", "admin_pass", "", "password");
  h += "</section>";

  h += "<section><h2>Погода / AQI (Open-Meteo)</h2>";
  h += "<label><input type=checkbox name=weather_enabled ";
  if (s.weather_enabled) h += "checked";
  h += "> Загружать погоду и AQI</label><div class=row>";
  field(h, "Широта", "wx_lat", s.weather_lat);
  field(h, "Долгота", "wx_lon", s.weather_lon);
  h += "</div></section>";

  h += "<section><h2>ICMP</h2><div class=row>";
  field(h, "GW", "ping_gw", s.ping_gw);
  field(h, "VPS", "ping_vps", s.ping_vps);
  field(h, "DNS", "ping_dns", s.ping_dns);
  h += "</div></section>";

  h += "<section><h2>OTA</h2>";
  field(h, "Manifest URL", "ota_url", s.ota_manifest_url);
  h += "</section>";

  h += "<button type=submit>Сохранить и перезагрузить</button></form>";
  h += page_foot("HTTP Basic передаётся по локальной сети без шифрования. "
                 "Не используйте этот интерфейс в недоверенной сети.");
  return h;
}

String WebServerService::update_page(const String& message) const {
  String h = page_head("tablo update");
  h += "<h1>Обновление прошивки</h1><p>Загрузите <code>firmware.bin</code> из CI "
       "или локальной сборки. На время записи подсветка выключается, LVGL "
       "приостанавливается.</p>";
  if (message.length()) h += "<p class=ok>" + esc(message) + "</p>";
  h += "<section><form method=post action=/update enctype=multipart/form-data>"
       "<input type=file name=firmware accept='.bin,application/octet-stream' required>"
       "<button type=submit>Загрузить и перезагрузить</button></form></section>";
  h += "<p><a class=btn href=/logs>Логи</a>";
  if (!rescue_mode_) h += "<a class=btn href=/config>Настройки</a>";
  h += "</p>";
  h += page_foot("Разделы OTA: ota_0 / ota_1 по ~5.94 MB.");
  return h;
}

void WebServerService::handle_config_get() {
  if (!authorized()) {
    send_auth_required();
    return;
  }
  server_.send(200, "text/html; charset=utf-8", config_page());
}

void WebServerService::handle_config_post() {
  if (!authorized()) {
    send_auth_required();
    return;
  }
  Settings s = settings_.get();
  copy_arg(server_.arg("wifi_ssid"), s.wifi_ssid, sizeof(s.wifi_ssid));
  copy_arg(server_.arg("wifi_pass"), s.wifi_password, sizeof(s.wifi_password), true);
  copy_arg(server_.arg("admin_user"), s.admin_user, sizeof(s.admin_user));
  copy_arg(server_.arg("admin_pass"), s.admin_password, sizeof(s.admin_password), true);
  copy_arg(server_.arg("tz"), s.timezone, sizeof(s.timezone));
  copy_arg(server_.arg("wx_lat"), s.weather_lat, sizeof(s.weather_lat));
  copy_arg(server_.arg("wx_lon"), s.weather_lon, sizeof(s.weather_lon));
  copy_arg(server_.arg("ping_gw"), s.ping_gw, sizeof(s.ping_gw));
  copy_arg(server_.arg("ping_vps"), s.ping_vps, sizeof(s.ping_vps));
  copy_arg(server_.arg("ping_dns"), s.ping_dns, sizeof(s.ping_dns));
  copy_arg(server_.arg("ota_url"), s.ota_manifest_url, sizeof(s.ota_manifest_url));

  s.schedule.enabled = server_.hasArg("schedule_enabled");
  if (!parse_hhmm(server_.arg("evening_start").c_str(), s.schedule.evening_min) ||
      !parse_hhmm(server_.arg("night_start").c_str(), s.schedule.night_min) ||
      !parse_hhmm(server_.arg("day_start").c_str(), s.schedule.day_min) ||
      !night_schedule_valid(s.schedule)) {
    server_.send(400, "text/html; charset=utf-8",
                 config_page("Расписание: укажите время как HH:MM и порядок день < вечер < ночь"));
    return;
  }
  copy_arg(server_.arg("note"), s.note, sizeof(s.note));

  const int bright = server_.arg("bright").toInt();
  if (bright >= 10 && bright <= 100) s.brightness = static_cast<uint8_t>(bright);
  s.auto_ota = server_.hasArg("auto_ota");
  s.weather_enabled = server_.hasArg("weather_enabled");
  const int date_format = server_.arg("date_format").toInt();
  if (date_format >= 0 && date_format <= 2) {
    s.date_format = static_cast<DateFormat>(date_format);
  }

  if (s.admin_user[0] == '\0' || s.admin_password[0] == '\0') {
    server_.send(400, "text/html; charset=utf-8",
                 config_page("Логин и пароль не могут быть пустыми"));
    return;
  }
  if (std::strlen(s.admin_password) < 8 ||
      std::strcmp(s.admin_password, "change-me") == 0) {
    server_.send(400, "text/html; charset=utf-8",
                 config_page("Пароль администратора — минимум 8 символов"));
    return;
  }

  if (!settings_.save_checked(s)) {
    server_.send(500, "text/html; charset=utf-8",
                 config_page("Не удалось сохранить настройки в NVS"));
    return;
  }
  LOGI("web: config saved, rebooting");
  server_.send(200, "text/html; charset=utf-8",
               "<!doctype html><meta charset=utf-8><meta http-equiv=refresh content='2;url=/config'>"
               "<body style='background:#0b0f14;color:#e7edf5;font:18px system-ui;padding:40px'>"
               "Сохранено. Перезагрузка…</body>");
  delay(900);
  ESP.restart();
}

void WebServerService::handle_update_get() {
  if (!authorized()) {
    send_auth_required();
    return;
  }
  server_.send(200, "text/html; charset=utf-8", update_page());
}

void WebServerService::handle_logs() {
  if (!authorized()) {
    send_auth_required();
    return;
  }
  static char snapshot[100 * 160 + 512];
  log_snapshot(snapshot, sizeof(snapshot));
  server_.send(200, "text/plain; charset=utf-8", snapshot);
}

void WebServerService::handle_upload_write() {
  HTTPUpload& up = server_.upload();
  if (up.status == UPLOAD_FILE_START) {
    upload_allowed_ = authorized();
    upload_ok_ = false;
    if (!upload_allowed_) return;
    LOGI("update: upload start %s", up.filename.c_str());
    upload_paused_ = render_pause();
    if (!upload_paused_) {
      upload_ok_ = false;
      apply_current_backlight();
      return;
    }
    backlight_set(0);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      upload_ok_ = false;
    } else {
      upload_ok_ = true;
    }
  } else if (up.status == UPLOAD_FILE_WRITE && upload_allowed_) {
    if (upload_ok_ && Update.write(up.buf, up.currentSize) != up.currentSize) {
      Update.printError(Serial);
      upload_ok_ = false;
    }
  } else if (up.status == UPLOAD_FILE_END) {
    LOGI("update: upload end, %u bytes", static_cast<unsigned>(up.totalSize));
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    LOGE("update: upload aborted by client");
    if (upload_paused_) {
      Update.abort();
      render_resume();
      apply_current_backlight();
      upload_paused_ = false;
    }
    upload_allowed_ = false;
    upload_ok_ = false;
  }
}

void WebServerService::handle_upload_done() {
  if (!authorized()) {
    send_auth_required();
    return;
  }
  if (upload_allowed_ && upload_ok_ && !Update.hasError() && Update.end(true)) {
    LOGI("update: firmware written, rebooting");
    boot_guard_clear();
    server_.send(200, "text/html; charset=utf-8",
                 "<!doctype html><meta charset=utf-8><body style='background:#0b0f14;"
                 "color:#e7edf5;font:18px system-ui;padding:40px'>Прошивка записана. "
                 "Перезагрузка…</body>");
    delay(800);
    ESP.restart();
    return;
  }
  if (upload_paused_) {
    render_resume();
    apply_current_backlight();
    upload_paused_ = false;
  }
  server_.send(500, "text/html; charset=utf-8",
               update_page("Ошибка записи. Проверьте .bin и разделы OTA."));
}

}  // namespace tablo
