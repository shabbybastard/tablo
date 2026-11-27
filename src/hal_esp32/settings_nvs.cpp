#include "settings_nvs.h"

#include <Arduino.h>

#include <cstddef>
#include <cstring>

#include "log_ring.h"

namespace tablo {
namespace {
constexpr const char* kNamespace = "tablo";
constexpr const char* kBlobKey = "settings";
constexpr uint32_t kBlobMagic = 0x54424C53u;  // "TBLS"
constexpr uint16_t kBlobVersion = 3;
// Layout from the first public release: used only to migrate existing devices.
constexpr uint16_t kSettingsV1Size = 766;
constexpr size_t kBlobV1Size = 780;
// Version 2 added date_format and weather_enabled. The old struct was padded
// to 784 bytes on the 4-byte blob alignment, so the two tail-padding bytes
// overlap the first new fields when a v2 blob is read into the new struct.
constexpr uint16_t kSettingsV2Size = 768;
constexpr size_t kBlobV2Size = 784;
// Version 3 appended purifier fields; the v1/v2 prefixes must stay
// byte-identical for the migrations below.
static_assert(offsetof(Settings, date_format) == kSettingsV1Size,
              "new Settings fields must be appended after the v1 layout");
static_assert(offsetof(Settings, purifier_enabled) == kSettingsV2Size,
              "new Settings fields must be appended after the v2 layout");

struct SettingsBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sd_hash;
  bool sd_hash_valid;
  Settings settings;
};

static_assert(offsetof(SettingsBlob, settings) + kSettingsV1Size == kBlobV1Size,
              "v1 blob layout changed");
static_assert(offsetof(SettingsBlob, settings) + kSettingsV2Size <= kBlobV2Size &&
                  kBlobV2Size - (offsetof(SettingsBlob, settings) +
                                 kSettingsV2Size) <
                      4,
              "v2 blob layout changed");

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
  if (migrated_) {
    if (write_blob_locked()) {
      migrated_ = false;
      LOGI("nvs: settings blob migrated to v%u", kBlobVersion);
    } else {
      dirty_ = true;
      dirty_since_ = millis();
      LOGE("nvs: settings blob migration write failed");
    }
  }
  LOGI("nvs: namespace '%s' ready", kNamespace);
  return true;
}

void NvsSettings::load_locked() {
  const bool blob_present = prefs_.isKey(kBlobKey);
  bool blob_loaded = false;

  if (blob_present) {
    const size_t blob_length = prefs_.getBytesLength(kBlobKey);
    if (blob_length == sizeof(SettingsBlob)) {
      SettingsBlob blob{};
      if (prefs_.getBytes(kBlobKey, &blob, sizeof(blob)) == sizeof(blob) &&
          blob.magic == kBlobMagic && blob.version == kBlobVersion &&
          blob.size == sizeof(Settings)) {
        settings_ = blob.settings;
        sd_hash_ = blob.sd_hash;
        sd_hash_valid_ = blob.sd_hash_valid;
        blob_loaded = true;
      }
    } else if (blob_length == kBlobV2Size) {
      SettingsBlob blob{};
      if (prefs_.getBytes(kBlobKey, &blob, kBlobV2Size) == kBlobV2Size &&
          blob.magic == kBlobMagic && blob.version == 2 &&
          blob.size == kSettingsV2Size) {
        settings_ = blob.settings;
        // The two tail-padding bytes of the old blob landed in the first new
        // fields; make sure they are not interpreted as configuration.
        settings_.purifier_enabled = false;
        settings_.purifier_ip[0] = '\0';
        settings_.purifier_token[0] = '\0';
        sd_hash_ = blob.sd_hash;
        sd_hash_valid_ = blob.sd_hash_valid;
        migrated_ = true;
        blob_loaded = true;
      }
    } else if (blob_length == kBlobV1Size) {
      SettingsBlob blob{};
      if (prefs_.getBytes(kBlobKey, &blob, kBlobV1Size) == kBlobV1Size &&
          blob.magic == kBlobMagic && blob.version == 1 &&
          blob.size == kSettingsV1Size) {
        settings_ = blob.settings;
        settings_.date_format = DateFormat::DayMonthWeekday;
        settings_.weather_enabled = true;
        settings_.purifier_enabled = false;
        settings_.purifier_ip[0] = '\0';
        settings_.purifier_token[0] = '\0';
        sd_hash_ = blob.sd_hash;
        sd_hash_valid_ = blob.sd_hash_valid;
        migrated_ = true;
        blob_loaded = true;
      }
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

void NvsSettings::set_timezone(const char* timezone) {
  if (timezone == nullptr) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::strncmp(settings_.timezone, timezone, sizeof(settings_.timezone)) == 0) {
    return;
  }
  std::strncpy(settings_.timezone, timezone, sizeof(settings_.timezone) - 1);
  settings_.timezone[sizeof(settings_.timezone) - 1] = '\0';
  dirty_ = true;
  dirty_since_ = millis();
}

void NvsSettings::set_date_format(DateFormat format) {
  if (static_cast<uint8_t>(format) > 2) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (settings_.date_format == format) return;
  settings_.date_format = format;
  dirty_ = true;
  dirty_since_ = millis();
}

void NvsSettings::set_weather_enabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (settings_.weather_enabled == enabled) return;
  settings_.weather_enabled = enabled;
  dirty_ = true;
  dirty_since_ = millis();
}

void NvsSettings::set_external(bool enabled, const char* ip,
                               const char* token) {
  std::lock_guard<std::mutex> lock(mutex_);
  bool changed = false;

  if (settings_.purifier_enabled != enabled) {
    settings_.purifier_enabled = enabled;
    changed = true;
  }
  if (ip != nullptr && std::strcmp(settings_.purifier_ip, ip) != 0) {
    std::strncpy(settings_.purifier_ip, ip,
                 sizeof(settings_.purifier_ip) - 1);
    settings_.purifier_ip[sizeof(settings_.purifier_ip) - 1] = '\0';
    changed = true;
  }
  if (token != nullptr && token[0] != '\0' &&
      std::strcmp(settings_.purifier_token, token) != 0) {
    std::strncpy(settings_.purifier_token, token,
                 sizeof(settings_.purifier_token) - 1);
    settings_.purifier_token[sizeof(settings_.purifier_token) - 1] = '\0';
    changed = true;
  }

  if (changed) {
    dirty_ = true;
    dirty_since_ = millis();
  }
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
