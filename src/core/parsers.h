#pragma once

#include <cstddef>
#include <cstdint>

#include "app_state.h"

namespace tablo {

struct ManifestInfo {
  char version[24] = "";
  char bin_url[192] = "";
  char sha256[65] = "";
  uint32_t size = 0;
};

/** Strict-enough parser for the public OTA manifest. */
bool parse_manifest(const char* json, size_t len, ManifestInfo& out);

/** true when `candidate` is a higher dotted version than `current`. */
bool version_is_newer(const char* candidate, const char* current);

/** Open-Meteo current weather. */
bool parse_openmeteo_weather(const char* json, size_t len, WeatherInfo& out);

/** Open-Meteo air quality (PM2.5 + US AQI). */
bool parse_openmeteo_air(const char* json, size_t len, WeatherInfo& out);

const char* wmo_condition_ru(int code);

}  // namespace tablo
