#pragma once

#include <cstddef>
#include <cstdint>

#include "core/settings.h"
#include "sd_card.h"
#include "settings_nvs.h"

namespace tablo {

/**
 * Reads tablo-config.json from a hot-pluggable microSD card and validates it.
 * All methods run from the Arduino main loop.
 */
class SdConfigService {
 public:
  enum class EventType : uint8_t {
    None,
    CardMounted,
    CardRemoved,
    ConfigReady,
    ConfigInvalid,
  };

  struct Event {
    EventType type = EventType::None;
    Settings settings;
    uint32_t hash = 0;
    char file[48] = "";
    char error[192] = "";
  };

  explicit SdConfigService(NvsSettings& settings) : settings_(settings) {}

  void begin(bool enabled = true);
  void end();
  void poll();

  Event take_event();

  bool mounted() const { return card_.mounted(); }
  bool config_present() const { return config_state_ != ConfigState::None; }
  bool config_ready() const { return config_state_ == ConfigState::Ready; }
  bool config_invalid() const { return config_state_ == ConfigState::Invalid; }
  const Settings& parsed_settings() const { return parsed_; }
  uint32_t parsed_hash() const { return parsed_hash_; }
  const char* error() const { return error_; }

  bool write_example_if_missing();
  bool already_applied(uint32_t hash) const;
  bool remember_applied(uint32_t hash);

 private:
  enum class ConfigState : uint8_t {
    None,
    Ready,
    Invalid,
  };

  void scan_card();
  void reset_scan();

  NvsSettings& settings_;
  SdCard card_;
  Event event_;
  Settings parsed_;
  uint32_t parsed_hash_ = 0;
  ConfigState config_state_ = ConfigState::None;
  char error_[192] = "";
  bool was_mounted_ = false;
};

}  // namespace tablo
