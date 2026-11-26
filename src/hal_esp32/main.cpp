#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_psram.h>

#include <cstdio>
#include <cstring>

#include "backlight.h"
#include "board_pins.h"
#include "boot_guard.h"
#include "core/platform.h"
#include "core/settings_json.h"
#include "core/settings_validate.h"
#include "core/state_store.h"
#include "core/version.h"
#include "display.h"
#include "log_ring.h"
#include "net_worker.h"
#include "platform_esp32.h"
#include "qemu.h"
#include "render.h"
#include "sd_config.h"
#include "settings_nvs.h"
#include "setup_flow.h"
#include "touch.h"
#include "ui/ui.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace {
tablo::NvsSettings g_settings;
tablo::NetWorker* g_net = nullptr;
tablo::WifiManager* g_wifi = nullptr;
tablo::SdConfigService* g_sd = nullptr;
bool g_stable_announced = false;
uint32_t g_ready_ms = 0;

tablo::Settings g_pending_sd;
uint32_t g_pending_sd_hash = 0;
bool g_pending_sd_valid = false;
uint32_t g_sd_event_seq = 0;

uint32_t tick_cb() { return static_cast<uint32_t>(millis()); }

void copy_text(char* dst, size_t cap, const char* src) {
  std::strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

[[noreturn]] void rescue_mode(tablo::NvsSettings& settings) {
  LOGE("RESCUE MODE: LVGL and API workers are disabled");
  tablo::backlight_init();
  const bool display_ok = tablo::display_begin();
  tablo::backlight_set(display_ok ? 20 : 0);
  if (display_ok) {
    tablo::display_show_message("RESCUE MODE", "update / logs only", "wifi setup...");
  }
  tablo::WifiManager wifi(settings);
  wifi.begin(true, true);
  tablo::WebServerService web(settings);
  web.begin(true);
  while (true) {
    wifi.loop();
    web.loop();
    delay(10);
  }
}

void fill_initial_state(const tablo::Settings& settings) {
  tablo::AppState state;
  state.brightness = settings.brightness;
  state.schedule = settings.schedule;
  state.date_format = settings.date_format;
  state.weather_enabled = settings.weather_enabled;
  copy_text(state.timezone, sizeof(state.timezone), settings.timezone);
  copy_text(state.wifi_ssid, sizeof(state.wifi_ssid), settings.wifi_ssid);
  if (settings.schedule.enabled) {
    // The clock is not valid yet; the schedule's invalid-time fallback is Day.
    state.display_mode = tablo::DisplayMode::Day;
    state.night_mode = false;
  } else {
    state.night_mode = settings.night_mode;
    state.display_mode =
        settings.night_mode ? tablo::DisplayMode::Night : tablo::DisplayMode::Day;
  }
  copy_text(state.fw_version, sizeof(state.fw_version), tablo::kFwVersion);
  copy_text(state.note, sizeof(state.note), settings.note);
  state.ota.checked = false;
  snprintf(state.ota.status, sizeof(state.ota.status), "Проверка при подключении");
  tablo::g_state.set(state);
}

void publish_initial_setup(bool setup_active) {
  tablo::AppState state = tablo::g_state.snapshot();
  state.setup.active = setup_active;
  state.setup.mode = setup_active ? tablo::SetupMode::Choice
                                  : tablo::SetupMode::None;
  if (setup_active) {
    snprintf(state.setup.status, sizeof(state.setup.status),
             "Вставьте microSD с tablo-config.json или настройте Wi-Fi с телефона.");
    snprintf(state.setup.hint, sizeof(state.setup.hint),
             "Карту можно вставлять и извлекать без выключения платы.");
  }
  tablo::g_state.set(state);
}

void clear_sd_import() {
  g_pending_sd_valid = false;
  tablo::SdImportInfo info;
  tablo::g_state.update_sd_import(info);
}

void publish_sd_ready(const tablo::SdConfigService::Event& event) {
  g_pending_sd = event.settings;
  g_pending_sd_hash = event.hash;
  g_pending_sd_valid = true;

  tablo::SdImportInfo info;
  info.pending = true;
  info.invalid = false;
  info.event_id = ++g_sd_event_seq;
  copy_text(info.file, sizeof(info.file), event.file);
  tablo::g_state.update_sd_import(info);
  LOGI("sd: config pending confirmation, hash=%08lx",
       static_cast<unsigned long>(event.hash));
}

void publish_sd_invalid(const tablo::SdConfigService::Event& event) {
  g_pending_sd_valid = false;

  tablo::SdImportInfo info;
  info.pending = false;
  info.invalid = true;
  info.event_id = ++g_sd_event_seq;
  copy_text(info.file, sizeof(info.file), event.file);
  copy_text(info.error, sizeof(info.error), event.error);
  tablo::g_state.update_sd_import(info);
}

void publish_sd_save_error() {
  g_pending_sd_valid = false;

  tablo::SdImportInfo info;
  info.pending = false;
  info.invalid = true;
  info.event_id = ++g_sd_event_seq;
  copy_text(info.file, sizeof(info.file), tablo::kSettingsFileName);
  copy_text(info.error, sizeof(info.error),
            "Не удалось сохранить настройки в NVS. Извлеките и вставьте карту, "
            "чтобы повторить.");
  tablo::g_state.update_sd_import(info);
}

void apply_pending_sd() {
  if (!g_pending_sd_valid) {
    return;
  }
  tablo::SdImportInfo info = tablo::g_state.snapshot().sd_import;
  info.applying = true;
  tablo::g_state.update_sd_import(info);

  if (!g_settings.save_checked(g_pending_sd)) {
    LOGE("sd: failed to save UI-imported settings to NVS");
    publish_sd_save_error();
    return;
  }
  if (!g_settings.set_sd_config_hash(g_pending_sd_hash)) {
    LOGW("sd: settings saved, but config hash was not persisted");
  }
  tablo::boot_guard_mark_stable();
  LOGI("sd: config applied from UI, ssid='%s', hash=%08lx",
       g_pending_sd.wifi_ssid,
       static_cast<unsigned long>(g_pending_sd_hash));
  g_pending_sd_valid = false;
  if (g_sd != nullptr) {
    g_sd->end();
  }
  delay(1200);
  ESP.restart();
}

void poll_runtime_sd() {
  if (g_sd == nullptr) {
    return;
  }
  g_sd->poll();
  const tablo::SdConfigService::Event event = g_sd->take_event();
  switch (event.type) {
    case tablo::SdConfigService::EventType::ConfigReady:
      if (g_sd->already_applied(event.hash)) {
        LOGI("sd: config hash already applied, ignoring");
        clear_sd_import();
      } else {
        publish_sd_ready(event);
      }
      break;
    case tablo::SdConfigService::EventType::ConfigInvalid:
      publish_sd_invalid(event);
      break;
    case tablo::SdConfigService::EventType::CardMounted:
    case tablo::SdConfigService::EventType::CardRemoved:
      clear_sd_import();
      break;
    default:
      break;
  }
}

void handle_ui_actions() {
  const uint32_t actions = tablo::platform_take_actions();
  if ((actions & (1u << static_cast<uint32_t>(
                      tablo::PlatformAction::SdImportApply))) != 0) {
    apply_pending_sd();
  }
  if ((actions & (1u << static_cast<uint32_t>(
                      tablo::PlatformAction::SdImportDismiss))) != 0) {
    clear_sd_import();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  tablo::log_init();
  LOGI("tablo %s boot", tablo::kFwVersion);

  tablo::boot_guard_begin();
  if (!g_settings.begin()) {
    LOGE("settings: NVS init failed, using defaults");
  }

  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running != nullptr) {
    LOGI("boot: running from %s at 0x%06lx", running->label,
         static_cast<unsigned long>(running->address));
  }
  const size_t psram_bytes = esp_psram_get_size();
  LOGI("boot: PSRAM %u MB", static_cast<unsigned>(psram_bytes / (1024U * 1024U)));
  if (tablo::qemu_detected()) {
    LOGI("boot: Espressif QEMU esp32s3 machine detected");
  }

  if (tablo::boot_guard_in_rescue()) {
    rescue_mode(g_settings);
  }

  const tablo::Settings settings = g_settings.get();
  tablo::backlight_init();
  tablo::backlight_set(0);

  const bool display_ok = tablo::display_begin();
  if (display_ok) {
    tablo::display_show_message("tablo", "starting...", "wifi / display");
  } else {
    LOGE("display: init failed, continuing without UI");
  }
  const tablo::DisplayMode initial_mode =
      settings.schedule.enabled
          ? tablo::display_mode_for(settings.schedule, 0, false)
          : (settings.night_mode ? tablo::DisplayMode::Night
                                 : tablo::DisplayMode::Day);
  tablo::backlight_set(tablo::brightness_for_mode(settings.brightness, initial_mode));

  bool ui_ready = false;
  bool touch_ok = false;
  bool setup_active = false;

  if (display_ok) {
    lv_init();
    lv_tick_set_cb(tick_cb);
    if (!tablo::display_lvgl_create()) {
      LOGE("display: LVGL display init failed");
    } else {
      fill_initial_state(settings);
      setup_active = tablo::settings_need_setup(settings) && !tablo::qemu_detected();
      publish_initial_setup(setup_active);

      touch_ok = tablo::touch_begin();
      if (!touch_ok) {
        LOGW("touch: controller not found, UI is read-only");
      }
      tablo::ui_init(setup_active);
      tablo::touch_lvgl_create();
      tablo::platform_bind(&g_settings, nullptr);
      tablo::render_task_start();
      LOGI("lvgl: UI ready %dx%d", tablo::board::LCD_WIDTH,
           tablo::board::LCD_HEIGHT);
      ui_ready = true;
    }
  }
  if (!ui_ready) {
    fill_initial_state(settings);
  }

  if (setup_active) {
    g_wifi = new tablo::WifiManager(g_settings);
    if (!touch_ok) {
      LOGW("setup: touch unavailable, starting Wi-Fi setup automatically");
      tablo::platform().requestAction(tablo::PlatformAction::SetupChooseWifi);
    }
    tablo::run_setup_flow(g_settings, *g_wifi);
  }

  if (g_wifi == nullptr) {
    g_wifi = new tablo::WifiManager(g_settings);
  }
  g_wifi->begin(true, false);
  tablo::g_state.update_wifi(g_wifi->connected(),
                             static_cast<int8_t>(g_wifi->rssi()),
                             g_wifi->ip().c_str());

  g_net = new tablo::NetWorker(g_settings, *g_wifi);
  tablo::platform_bind(&g_settings, g_net);
  g_net->begin();

  if (ui_ready && !tablo::qemu_detected()) {
    g_sd = new tablo::SdConfigService(g_settings);
    g_sd->begin(true);
  }

  if (ui_ready) {
    tablo::ui_update(tablo::g_state.snapshot());
  }
  g_ready_ms = millis();
  LOGI("boot: services started");
}

void loop() {
  if (!g_stable_announced && g_ready_ms != 0 &&
      static_cast<uint32_t>(millis() - g_ready_ms) > 10000U) {
    tablo::boot_guard_mark_stable();
    g_stable_announced = true;
  }
  g_settings.tick();
  poll_runtime_sd();
  handle_ui_actions();
  delay(200);
}
