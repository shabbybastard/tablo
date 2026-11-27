#include <lvgl.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/platform.h"
#include "core/state_store.h"
#include "hal_wasm/mock_provider.h"
#include "ui/ui.h"

namespace tablo {
int test_platform_reboots();
int test_platform_last_action();
int test_platform_external_calls();
}

namespace {

constexpr int kWidth = 480;
constexpr int kHeight = 480;
constexpr uint32_t kFrameMs = 16;

alignas(64) uint16_t g_framebuffer[kWidth * kHeight];
uint32_t g_tick_ms = 0;
int g_errors = 0;
int g_flush_count = 0;

uint32_t tick_cb() { return g_tick_ms; }

void flush_cb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) {
  if (area == nullptr || px_map == nullptr) {
    std::printf("FAIL flush: null area or pixel map\n");
    ++g_errors;
  } else if (area->x1 < 0 || area->y1 < 0 || area->x2 >= kWidth ||
             area->y2 >= kHeight || area->x1 > area->x2 ||
             area->y1 > area->y2) {
    std::printf("FAIL flush: area out of bounds (%d,%d)-(%d,%d)\n", area->x1,
                area->y1, area->x2, area->y2);
    ++g_errors;
  } else {
    ++g_flush_count;
  }
  lv_display_flush_ready(display);
}

void check(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) {
    ++g_errors;
  }
}

void render_frames(int frames) {
  for (int i = 0; i < frames; ++i) {
    g_tick_ms += kFrameMs;
    lv_timer_handler();
  }
}

bool framebuffer_has_content() {
  size_t non_black = 0;
  for (uint16_t pixel : g_framebuffer) {
    if (pixel != 0) {
      ++non_black;
    }
  }
  // A dark theme still fills the screen with a non-zero RGB565 background.
  return non_black > static_cast<size_t>(kWidth) * 32;
}

uint32_t framebuffer_checksum() {
  uint32_t hash = 2166136261u;
  for (uint16_t pixel : g_framebuffer) {
    hash ^= pixel;
    hash *= 16777619u;
  }
  return hash;
}

}  // namespace

int main() {
  std::printf("UI smoke test: mock=%s\n", TABLO_MOCK_DATA_PATH);

  tablo::mock_provider().begin(TABLO_MOCK_DATA_PATH);
  check(tablo::mock_provider().loaded(), "mock_data.json loaded");
  check(tablo::mock_provider().state().weather.condition[0] != '\0',
        "mock_provider state populated");

  tablo::AppState state = tablo::g_state.snapshot();
  check(std::strcmp(state.fw_version, "1.0.6") == 0, "fw_version from mock");
  check(state.weather.valid, "weather valid");
  check(state.wifi_connected && std::strcmp(state.ip, "192.0.2.42") == 0,
        "wifi/ip from mock");
  check(state.external.valid && state.external.pm25 == 8,
        "purifier data from mock");

  lv_init();
  lv_tick_set_cb(tick_cb);

  lv_display_t* display = lv_display_create(kWidth, kHeight);
  check(display != nullptr, "lv_display_create");
  if (display == nullptr) {
    return 1;
  }
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, flush_cb);
  lv_display_set_buffers(display, g_framebuffer, nullptr, sizeof(g_framebuffer),
                         LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_default(display);

  tablo::ui_init();
  check(lv_screen_active() != nullptr, "ui_init created a screen");
  tablo::ui_update(state);

  render_frames(40);
  check(g_flush_count > 0, "dashboard flushed at least once");
  check(framebuffer_has_content(), "dashboard rendered visible pixels");
  const uint32_t dashboard_hash = framebuffer_checksum();
  lv_obj_t* dashboard = lv_screen_active();

  tablo::ui_test_show_settings();
  tablo::ui_update(state);
  render_frames(40);
  check(lv_screen_active() != dashboard, "settings screen loaded");
  check(framebuffer_checksum() != dashboard_hash,
        "settings screen changed the framebuffer");
  check(framebuffer_has_content(), "settings screen rendered visible pixels");
  const uint32_t settings_hash = framebuffer_checksum();

  state.schedule.enabled = true;
  state.schedule.evening_min = 18U * 60U;
  state.schedule.night_min = 23U * 60U;
  state.schedule.day_min = 7U * 60U;
  state.display_mode = tablo::DisplayMode::Evening;
  tablo::g_state.set(state);
  tablo::ui_test_show_schedule();
  tablo::ui_update(state);
  render_frames(30);
  check(lv_screen_active() != dashboard, "schedule screen loaded");
  check(framebuffer_has_content(), "schedule screen rendered visible pixels");
  check(framebuffer_checksum() != settings_hash,
        "schedule screen changed the framebuffer");
  state.display_mode = tablo::DisplayMode::Day;
  state.schedule.enabled = false;
  tablo::g_state.set(state);

  tablo::ui_test_show_options();
  tablo::ui_update(state);
  render_frames(30);
  check(lv_screen_active() != dashboard, "options screen loaded");
  check(framebuffer_has_content(), "options screen rendered visible pixels");
  const uint32_t options_hash = framebuffer_checksum();
  check(options_hash != settings_hash, "options screen changed the framebuffer");

  tablo::ui_test_show_system();
  tablo::ui_update(state);
  render_frames(30);
  check(lv_screen_active() != dashboard, "system screen loaded");
  check(framebuffer_has_content(), "system screen rendered visible pixels");

  tablo::ui_test_show_external();
  tablo::ui_update(state);
  render_frames(30);
  check(lv_screen_active() != dashboard, "external screen loaded");
  check(framebuffer_has_content(), "external screen rendered visible pixels");
  const uint32_t external_hash = framebuffer_checksum();
  check(external_hash != options_hash,
        "external screen changed the framebuffer");

  tablo::ui_test_edit_external_ip();
  render_frames(10);
  check(tablo::ui_test_external_modal_visible(), "external IP modal shown");
  tablo::ui_test_set_external_text("192.0.2.60");
  tablo::ui_test_save_external();
  render_frames(10);
  check(!tablo::ui_test_external_modal_visible(),
        "external IP modal saved and closed");
  check(std::strcmp(tablo::g_state.snapshot().external.ip, "192.0.2.60") == 0,
        "external IP saved");

  tablo::ui_test_edit_external_token();
  render_frames(10);
  check(tablo::ui_test_external_modal_visible(), "external token modal shown");
  tablo::ui_test_set_external_text("bad-token");
  tablo::ui_test_save_external();
  render_frames(5);
  check(tablo::ui_test_external_modal_visible(),
        "invalid token keeps the modal open");
  tablo::ui_test_set_external_text("0123456789abcdef0123456789abcdef");
  tablo::ui_test_save_external();
  render_frames(10);
  check(!tablo::ui_test_external_modal_visible(),
        "valid token closes the modal");
  check(tablo::g_state.snapshot().external.token_set,
        "external token saved");
  check(tablo::test_platform_external_calls() >= 2,
        "external platform setter called");
  tablo::ui_update(tablo::g_state.snapshot());
  render_frames(10);
  check(framebuffer_has_content(), "external screen updated after edits");

  tablo::ui_test_show_wifi();
  tablo::ui_update(tablo::g_state.snapshot());
  render_frames(40);
  check(lv_screen_active() != dashboard, "wifi screen loaded");
  check(framebuffer_has_content(), "wifi screen rendered visible pixels");
  check(framebuffer_checksum() != options_hash,
        "wifi screen changed the framebuffer");

  tablo::ui_test_click_wifi_connect();
  render_frames(40);
  check(tablo::ui_test_wifi_password_visible(), "wifi password modal shown");
  check(framebuffer_has_content(), "wifi password modal rendered");
  tablo::ui_test_dismiss_wifi_password();
  render_frames(10);
  check(!tablo::ui_test_wifi_password_visible(),
        "wifi password modal dismissed");

  tablo::ui_test_show_settings();
  tablo::ui_update(state);
  render_frames(10);

  tablo::ui_test_show_update_modal("1.0.6", "Доступна новая прошивка");
  tablo::ui_update(state);
  render_frames(40);
  check(tablo::ui_test_update_modal_visible(), "OTA modal is visible");
  check(framebuffer_checksum() != settings_hash,
        "OTA modal changed the framebuffer");

  state.ota.available = true;
  state.ota.downloading = true;
  state.ota.progress = 42;
  std::snprintf(state.ota.version, sizeof(state.ota.version), "1.0.6");
  std::snprintf(state.ota.status, sizeof(state.ota.status),
                "Загрузка обновления: 42%%");
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(20);
  check(tablo::ui_test_update_modal_visible(), "OTA modal survives progress");
  check(tablo::ui_test_version_highlighted(),
        "version is highlighted while OTA is active");

  state.ota.available = false;
  state.ota.downloading = false;
  state.ota.progress = 100;
  std::snprintf(state.fw_version, sizeof(state.fw_version), "1.0.6");
  std::snprintf(state.ota.status, sizeof(state.ota.status),
                "Установлено 1.0.6");
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(20);
  check(tablo::ui_test_update_modal_visible(), "OTA modal shows completion");
  check(!tablo::ui_test_version_highlighted(),
        "version highlight clears when OTA is inactive");

  state.night_mode = true;
  state.display_mode = tablo::DisplayMode::Night;
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(20);
  check(framebuffer_has_content(), "night theme rendered");

  state.night_mode = false;
  state.display_mode = tablo::DisplayMode::Day;
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(10);

  tablo::ui_test_hide_update_modal();
  tablo::ui_update(state);
  render_frames(10);
  check(!tablo::ui_test_update_modal_visible(), "OTA modal hidden");

  tablo::ui_test_show_dashboard();
  tablo::ui_update(state);
  render_frames(20);
  check(lv_screen_active() == dashboard, "dashboard restored");

  tablo::ui_test_click_dashboard_menu();
  render_frames(40);
  check(lv_screen_active() != dashboard, "dashboard menu opens settings");
  tablo::ui_test_show_dashboard();
  tablo::ui_update(state);
  render_frames(10);

  state.setup.active = true;
  state.setup.mode = tablo::SetupMode::Choice;
  std::snprintf(state.setup.status, sizeof(state.setup.status),
                "Вставьте microSD или настройте Wi-Fi");
  tablo::g_state.set(state);
  tablo::ui_test_show_setup();
  tablo::ui_update(state);
  render_frames(30);
  check(lv_screen_active() != dashboard, "setup screen loaded");
  check(framebuffer_has_content(), "setup screen rendered visible pixels");
  const uint32_t setup_hash = framebuffer_checksum();

  tablo::ui_test_click_setup_wifi();
  check(tablo::test_platform_last_action() ==
            static_cast<int>(tablo::PlatformAction::SetupChooseWifi),
        "setup Wi-Fi button reports an action");
  tablo::ui_test_click_setup_sd();
  check(tablo::test_platform_last_action() ==
            static_cast<int>(tablo::PlatformAction::SetupChooseSd),
        "setup microSD button reports an action");

  state.setup.mode = tablo::SetupMode::Sd;
  state.setup.card_mounted = true;
  std::snprintf(state.setup.status, sizeof(state.setup.status),
                "Шаблон tablo-config.example.json готов");
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(30);
  check(framebuffer_checksum() != setup_hash, "setup SD mode changed the framebuffer");

  state.setup.active = false;
  state.setup.mode = tablo::SetupMode::None;
  state.sd_import.pending = true;
  state.sd_import.invalid = false;
  state.sd_import.event_id = 101;
  std::snprintf(state.sd_import.file, sizeof(state.sd_import.file),
                "tablo-config.json");
  tablo::g_state.set(state);
  tablo::ui_test_show_dashboard();
  tablo::ui_update(state);
  render_frames(30);
  check(tablo::ui_test_sd_import_modal_visible(), "SD import modal shown");
  check(framebuffer_has_content(), "SD import modal rendered");

  state.sd_import.pending = false;
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(10);
  check(!tablo::ui_test_sd_import_modal_visible(), "SD import modal hidden");

  state.sd_import.invalid = true;
  state.sd_import.event_id = 102;
  std::snprintf(state.sd_import.error, sizeof(state.sd_import.error),
                "admin_password: минимум 8 символов");
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(20);
  check(tablo::ui_test_sd_import_modal_visible(), "invalid SD config modal shown");

  state.sd_import.invalid = false;
  tablo::g_state.set(state);
  tablo::ui_update(state);
  render_frames(10);
  check(!tablo::ui_test_sd_import_modal_visible(), "invalid SD config modal hidden");

  tablo::platform().requestAction(tablo::PlatformAction::SetupChooseSd);
  check(tablo::test_platform_last_action() ==
            static_cast<int>(tablo::PlatformAction::SetupChooseSd),
        "platform::requestAction reaches test HAL");

  tablo::platform().setBrightness(55);
  check(tablo::g_state.snapshot().brightness == 55,
        "platform::setBrightness updates state");
  tablo::platform().setNightMode(true);
  check(tablo::g_state.snapshot().night_mode, "platform::setNightMode updates state");
  tablo::platform().checkForUpdates();
  check(tablo::g_state.snapshot().ota.checked,
        "platform::checkForUpdates updates state");
  tablo::platform().installUpdate();
  check(tablo::g_state.snapshot().ota.downloading,
        "platform::installUpdate updates state");
  tablo::platform().reboot();
  check(tablo::test_platform_reboots() == 1, "platform::reboot reached test HAL");

  check(g_errors == 0, "no UI errors");
  std::printf("UI smoke test: %s (%d flushes)\n", g_errors == 0 ? "OK" : "FAILED",
              g_flush_count);
  return g_errors == 0 ? 0 : 1;
}
