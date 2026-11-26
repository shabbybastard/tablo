#include "sd_config.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "core/settings_json.h"
#include "log_ring.h"

namespace tablo {
namespace {
constexpr const char kConfigPath[] = "/sdcard/tablo-config.json";
constexpr const char kExamplePath[] = "/sdcard/tablo-config.example.json";
}  // namespace

void SdConfigService::begin(bool enabled) {
  card_.begin(enabled);
  was_mounted_ = false;
  reset_scan();
  event_ = Event();
}

void SdConfigService::end() {
  card_.end();
  was_mounted_ = false;
  reset_scan();
}

void SdConfigService::reset_scan() {
  config_state_ = ConfigState::None;
  parsed_hash_ = 0;
  error_[0] = '\0';
}

void SdConfigService::poll() {
  card_.poll();
  const bool now_mounted = card_.mounted();
  if (now_mounted == was_mounted_) {
    return;
  }
  was_mounted_ = now_mounted;

  if (now_mounted) {
    LOGI("sd: scanning for %s", kSettingsFileName);
    event_ = Event();
    event_.type = EventType::CardMounted;
    std::snprintf(event_.file, sizeof(event_.file), "%s", kSettingsFileName);
    scan_card();
  } else {
    LOGI("sd: card removed");
    reset_scan();
    event_ = Event();
    event_.type = EventType::CardRemoved;
  }
}

SdConfigService::Event SdConfigService::take_event() {
  Event result = event_;
  event_ = Event();
  return result;
}

void SdConfigService::scan_card() {
  reset_scan();
  if (!card_.mounted() || !card_.exists(kConfigPath)) {
    return;
  }

  auto buffer = std::unique_ptr<char[]>(
      new (std::nothrow) char[kSettingsJsonMaxBytes + 1]);
  if (!buffer) {
    config_state_ = ConfigState::Invalid;
    std::snprintf(error_, sizeof(error_), "Недостаточно памяти для чтения файла");
    LOGE("sd: %s", error_);
  } else {
    size_t length = 0;
    const SdCard::IoResult read = card_.read_file(
        kConfigPath, buffer.get(), kSettingsJsonMaxBytes + 1, length);
    if (read == SdCard::IoResult::TooLarge) {
      config_state_ = ConfigState::Invalid;
      std::snprintf(error_, sizeof(error_), "Файл больше 8 КиБ");
    } else if (read != SdCard::IoResult::Ok) {
      config_state_ = ConfigState::Invalid;
      std::snprintf(error_, sizeof(error_), "Не удалось прочитать файл с карты");
    } else {
      Settings parsed;
      char error[192] = {};
      if (parse_settings_json(buffer.get(), length, parsed, error, sizeof(error))) {
        parsed_ = parsed;
        parsed_hash_ = settings_config_hash(buffer.get(), length);
        config_state_ = ConfigState::Ready;
      } else {
        config_state_ = ConfigState::Invalid;
        std::snprintf(error_, sizeof(error_), "%s", error);
      }
    }
  }

  if (config_state_ == ConfigState::Ready) {
    LOGI("sd: config valid, hash=%08lx", static_cast<unsigned long>(parsed_hash_));
    event_.type = EventType::ConfigReady;
    event_.settings = parsed_;
    event_.hash = parsed_hash_;
    std::snprintf(event_.file, sizeof(event_.file), "%s", kSettingsFileName);
  } else if (config_state_ == ConfigState::Invalid) {
    LOGE("sd: config invalid: %s", error_);
    event_.type = EventType::ConfigInvalid;
    event_.hash = parsed_hash_;
    std::snprintf(event_.file, sizeof(event_.file), "%s", kSettingsFileName);
    std::snprintf(event_.error, sizeof(event_.error), "%s", error_);
  }
}

bool SdConfigService::write_example_if_missing() {
  if (!card_.mounted()) {
    return false;
  }
  if (card_.exists(kConfigPath)) {
    return true;
  }
  if (card_.exists(kExamplePath)) {
    LOGI("sd: %s already exists", kSettingsExampleFileName);
    return true;
  }
  const char* example = settings_example_json();
  const size_t length = std::strlen(example);
  if (!card_.write_file_if_missing(kExamplePath, example, length)) {
    LOGE("sd: failed to write %s", kSettingsExampleFileName);
    return false;
  }
  LOGI("sd: wrote %s", kSettingsExampleFileName);
  return true;
}

bool SdConfigService::already_applied(uint32_t hash) const {
  return settings_.has_sd_config_hash() && settings_.sd_config_hash() == hash;
}

bool SdConfigService::remember_applied(uint32_t hash) {
  return settings_.set_sd_config_hash(hash);
}

}  // namespace tablo
