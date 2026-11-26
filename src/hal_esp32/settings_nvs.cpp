#include "settings_nvs.h"

#include <Arduino.h>
#include <cstring>

#include "log_ring.h"

namespace tablo {
namespace {
constexpr const char* kNamespace = "tablo";
constexpr const char* kBlobKey = "settings";
constexpr uint32_t kBlobMagic = 0x54424C53u;  // "TBLS"
constexpr uint16_t kBlobVersion = 1;

struct SettingsBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sd_hash;
  bool sd_hash_valid;
  Settings settings;
};

void copy_str(char* dst, size_t cap, const String& value) {
  if (cap == 0) return;
  std::strncpy(dst, value.c_str(), cap - 1);
  dst[cap - 1] = '\0';
}

void read_str(Preferences& prefs, const char* key, char* dst, size_t cap) {
  if (!prefs.isKey(key)) return;
  copy_str(dst, cap, prefs.getString(key, dst));
}
}  // namespace

bool NvsSettings::begin() {
  std::lock_guard<std::mutex> lock(mutex_);
  settings_ = default_settings();
  if (!prefs_.begin(kNamespace, false)) {
    return false;
  }
  load_locked();
  ready_ = true;
  LOGI("nvs: namespace '%s' ready", kNamespace);
  return true;
}

void NvsSettings::load_locked() {
  const bool blob_present = prefs_.isKey(kBlobKey);
  bool blob_loaded = false;

  if (blob_present && prefs_.getBytesLength(kBlobKey) == sizeof(SettingsBlob)) {
    SettingsBlob blob{};
    if (prefs_.getBytes(kBlobKey, &blob, sizeof(blob)) == sizeof(blob) &&
        blob.magic == kBlobMagic && blob.version == kBlobVersion &&
        blob.size == sizeof(Settings)) {
      settings_ = blob.settings;
      sd_hash_ = blob.sd_hash;
      sd_hash_valid_ = blob.sd_hash_valid;
      blob_loaded = true;
    }
  }

  if (!blob_loaded) {
    // Upgrade path from firmware that stored every setting under its own key.
    load_legacy_locked();
    if (!blob_present) {
      sd_hash_valid_ = prefs_.getBool("sd_cfg_valid", false);
      sd_hash_ = prefs_.getUInt("sd_cfg_hash", 0);
    } else {
      // A corrupt blob must not resurrect a stale legacy hash.
      sd_hash_valid_ = false;
      sd_hash_ = 0;
    }
  }
}

void NvsSettings::load_legacy_locked() {
  read_str(prefs_, "wifi_ssid", settings_.wifi_ssid, sizeof(settings_.wifi_ssid));
  read_str(prefs_, "wifi_pass", settings_.wifi_password, sizeof(settings_.wifi_password));
  read_str(prefs_, "adm_user", settings_.admin_user, sizeof(settings_.admin_user));
  read_str(prefs_, "adm_pass", settings_.admin_password, sizeof(settings_.admin_password));
  read_str(prefs_, "tz", settings_.timezone, sizeof(settings_.timezone));
  read_str(prefs_, "wx_lat", settings_.weather_lat, sizeof(settings_.weather_lat));
  read_str(prefs_, "wx_lon", settings_.weather_lon, sizeof(settings_.weather_lon));
  read_str(prefs_, "ping_gw", settings_.ping_gw, sizeof(settings_.ping_gw));
  read_str(prefs_, "ping_vps", settings_.ping_vps, sizeof(settings_.ping_vps));
  read_str(prefs_, "ping_dns", settings_.ping_dns, sizeof(settings_.ping_dns));
  read_str(prefs_, "ota_url", settings_.ota_manifest_url, sizeof(settings_.ota_manifest_url));
  read_str(prefs_, "note", settings_.note, sizeof(settings_.note));
  settings_.brightness = prefs_.getUChar("bright", settings_.brightness);
  settings_.night_mode = prefs_.getBool("night", settings_.night_mode);
  settings_.auto_ota = prefs_.getBool("auto_ota", settings_.auto_ota);
}

Settings NvsSettings::get() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return settings_;
}

void NvsSettings::save(const Settings& settings) { (void)save_checked(settings); }

bool NvsSettings::write_blob_locked() {
  if (!ready_) {
    return false;
  }
  // One NVS blob keeps the update transactional: a failed commit leaves the
  // previous blob untouched instead of persisting a mix of old/new keys.
  SettingsBlob blob{};
  blob.magic = kBlobMagic;
  blob.version = kBlobVersion;
  blob.size = sizeof(Settings);
  blob.sd_hash = sd_hash_;
  blob.sd_hash_valid = sd_hash_valid_;
  blob.settings = settings_;
  return prefs_.putBytes(kBlobKey, &blob, sizeof(blob)) == sizeof(blob);
}

bool NvsSettings::save_checked_locked(const Settings& settings) {
  const Settings previous = settings_;
  const bool was_dirty = dirty_;
  settings_ = settings;
  if (!write_blob_locked()) {
    settings_ = previous;
    dirty_ = was_dirty;
    if (dirty_) {
      dirty_since_ = millis();
    }
    LOGE("settings: NVS blob write failed, keeping previous values");
    return false;
  }
  dirty_ = false;
  LOGI("settings: saved to NVS");
  return true;
}

bool NvsSettings::save_checked(const Settings& settings) {
  std::lock_guard<std::mutex> lock(mutex_);
  return save_checked_locked(settings);
}

void NvsSettings::set_brightness(uint8_t percent) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (settings_.brightness == percent) return;
  settings_.brightness = percent;
  dirty_ = true;
  dirty_since_ = millis();
}

void NvsSettings::set_night_mode(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (settings_.night_mode == enabled) return;
  settings_.night_mode = enabled;
  dirty_ = true;
  dirty_since_ = millis();
}

void NvsSettings::set_schedule(const NightSchedule& schedule) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (settings_.schedule.enabled == schedule.enabled &&
      settings_.schedule.evening_min == schedule.evening_min &&
      settings_.schedule.night_min == schedule.night_min &&
      settings_.schedule.day_min == schedule.day_min) {
    return;
  }
  settings_.schedule = schedule;
  dirty_ = true;
  dirty_since_ = millis();
}

bool NvsSettings::has_sd_config_hash() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return sd_hash_valid_;
}

uint32_t NvsSettings::sd_config_hash() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return sd_hash_;
}

bool NvsSettings::set_sd_config_hash(uint32_t hash) {
  std::lock_guard<std::mutex> lock(mutex_);
  const uint32_t previous_hash = sd_hash_;
  const bool previous_valid = sd_hash_valid_;
  sd_hash_ = hash;
  sd_hash_valid_ = true;
  if (!write_blob_locked()) {
    sd_hash_ = previous_hash;
    sd_hash_valid_ = previous_valid;
    LOGW("settings: failed to persist microSD config hash");
    return false;
  }
  return true;
}

void NvsSettings::tick() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!dirty_ || (millis() - dirty_since_) <= 900) {
    return;
  }
  // Keep the snapshot and the write under one lock so a render-task slider
  // change cannot be overwritten by a stale copy.
  if (!save_checked_locked(settings_)) {
    LOGW("settings: debounced save failed");
  }
}

}  // namespace tablo
