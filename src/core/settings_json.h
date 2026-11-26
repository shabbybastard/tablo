#pragma once

#include <cstddef>
#include <cstdint>

#include "core/settings.h"

namespace tablo {

inline constexpr const char kSettingsFileName[] = "tablo-config.json";
inline constexpr const char kSettingsExampleFileName[] = "tablo-config.example.json";
inline constexpr size_t kSettingsJsonMaxBytes = 8192;

/** Empty template written to microSD. Never contains secrets.h values. */
const char* settings_example_json();

/**
 * Parse and validate a provisioning JSON file.
 *
 * The file fully replaces the settings: missing keys keep defaults, empty
 * strings clear a value. On failure `error` contains a human-readable reason
 * and `out` must not be saved to NVS.
 */
bool parse_settings_json(const char* json, size_t len, Settings& out, char* error,
                         size_t error_cap);

/** FNV-1a hash used to remember which file was already applied. */
uint32_t settings_config_hash(const char* data, size_t len);

}  // namespace tablo
