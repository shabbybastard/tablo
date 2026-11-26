#pragma once

#include <Preferences.h>
#include <mutex>

#include "core/settings.h"

namespace tablo {

class NvsSettings : public ISettingsStore {
 public:
  bool begin();
  Settings get() const override;
  void save(const Settings& settings) override;

  /** Save and report whether every NVS key was committed. */
  bool save_checked(const Settings& settings);

  void set_brightness(uint8_t percent);
  void set_night_mode(bool enabled);
  void set_schedule(const NightSchedule& schedule);
  void tick();  // debounced NVS write for UI-driven runtime changes

  /** Hash of the last successfully applied microSD config (0 if none). */
  bool has_sd_config_hash() const;
  uint32_t sd_config_hash() const;
  bool set_sd_config_hash(uint32_t hash);

 private:
  void load_locked();
  void load_legacy_locked();
  bool write_blob_locked();
  bool save_checked_locked(const Settings& settings);

  mutable std::mutex mutex_;
  Preferences prefs_;
  Settings settings_;
  bool dirty_ = false;
  uint32_t dirty_since_ = 0;
  bool ready_ = false;
  bool sd_hash_valid_ = false;
  uint32_t sd_hash_ = 0;
};

}  // namespace tablo
