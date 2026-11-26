#pragma once

#include <cstdint>

#include "core/app_state.h"

namespace tablo {

/**
 * Data provider for the Emscripten preview.
 *
 * `begin()` loads `/mock_data.json` (preloaded next to tablo.js) into an
 * AppState and publishes it through g_state. Each `refresh()` keeps the clock
 * aligned with the browser, derives the display mode from the schedule and
 * processes the simulated OTA requests coming from platform_wasm.cpp.
 */
class MockProvider {
 public:
  /**
   * Loads the preview state from `mock_path` and publishes it via g_state.
   * The WASM build keeps the default `/mock_data.json` preloaded path; the
   * native UI smoke test passes an absolute path to preview/mock_data.json.
   */
  void begin(const char* mock_path = "/mock_data.json");

  /** Refreshes the preview data and copies the result into `state`. */
  bool refresh(AppState& state);

  /** Simulates an OTA manifest check on the next refresh. */
  void requestUpdateCheck();

  /** Starts the simulated download/install animation. */
  void requestInstallUpdate();

  void setBrightness(uint8_t percent);
  void setNightMode(bool on);
  void setSchedule(const NightSchedule& schedule);

  const AppState& state() const { return state_; }
  bool loaded() const { return loaded_; }

 private:
  bool load(const char* mock_path);
  void applyFallback();
  void updateClock();
  void updateDisplayMode();
  void processRequests();
  uint32_t nextRandom();

  AppState state_{};
  char mock_path_[256] = "/mock_data.json";
  char ota_version_[24] = "1.0.0";
  bool loaded_ = false;
  bool update_check_pending_ = false;
  bool install_pending_ = false;
  uint32_t tick_count_ = 0;
  uint32_t rng_ = 0x9e3779b9u;
};

/** Process-wide preview provider shared with the WASM platform glue. */
MockProvider& mock_provider();

}  // namespace tablo
