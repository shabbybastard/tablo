#pragma once

#include <cstddef>
#include <cstdint>

namespace tablo {

constexpr size_t kMiioTokenBytes = 16;
constexpr size_t kMiioTokenHexChars = 32;

/**
 * Property layout for one Xiaomi air purifier family.
 *
 * Most purifiers use the MIoT `get_properties` command with siid/piid pairs.
 * Older 2/2S/Pro models use `get_prop` with string property names instead.
 * A zero siid/piid means the property is not available on that model.
 */
struct MiioMapping {
  const char* family;
  bool legacy_get_prop;
  uint8_t power_siid;
  uint8_t power_piid;
  uint8_t mode_siid;
  uint8_t mode_piid;
  uint8_t fan_siid;
  uint8_t fan_piid;
  uint8_t humidity_siid;
  uint8_t humidity_piid;
  uint8_t temperature_siid;
  uint8_t temperature_piid;
  uint8_t aqi_siid;
  uint8_t aqi_piid;
  uint8_t filter_siid;
  uint8_t filter_piid;
  uint8_t tvoc_siid;
  uint8_t tvoc_piid;
};

/** Values decoded from one device response. Missing values stay negative. */
struct MiioReading {
  bool has_power = false;
  bool power = false;
  int16_t pm25 = -1;
  int16_t temperature = 0;
  bool has_temperature = false;
  int16_t humidity = 0;
  bool has_humidity = false;
  int16_t filter_life = -1;
  int16_t fan_level = -1;
  int16_t mode = -1;
  int16_t tvoc = -1;
};

/** Parse a 32-character hex token into 16 bytes. */
bool miio_parse_token(const char* text, uint8_t out[kMiioTokenBytes]);

/** Strict dotted-quad IPv4 check (no hostnames, no spaces). */
bool miio_valid_ipv4(const char* text);

/** Pick the property layout for a `miIO.info` model name. */
const MiioMapping* miio_mapping_for_model(const char* model);

/** True when the model is in one of the explicitly known families. */
bool miio_model_supported(const char* model);

/** Build a full `get_properties` request JSON (without a trailing NUL). */
size_t miio_build_get_properties(uint16_t id, const MiioMapping& mapping,
                                 char* out, size_t cap);

/** Build a full legacy `get_prop` request JSON (without a trailing NUL). */
size_t miio_build_get_prop(uint16_t id, char* out, size_t cap);

/** Extract `result.model` from a `miIO.info` response. */
bool miio_parse_model(const char* json, size_t len, char* out, size_t cap);

/** Decode a MIoT `get_properties` response. */
bool miio_parse_miot_properties(const char* json, size_t len, MiioReading& out);

/** Decode a legacy `get_prop` response. */
bool miio_parse_legacy_props(const char* json, size_t len, MiioReading& out);

/** Human-readable operation mode (0 auto, 1 silent, 2 favorite, 3 fan). */
const char* miio_mode_text(int mode);

/** Short Russian quality label for a PM2.5 value in µg/m³. */
const char* miio_quality_text(int pm25);

}  // namespace tablo
