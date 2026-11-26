#include "parsers.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "json.h"

namespace tablo {
namespace {

constexpr size_t kMaxVersionParts = 8;

void copy_text(char* dst, size_t cap, const char* src) {
  if (dst == nullptr || cap == 0) {
    return;
  }
  if (src == nullptr) {
    src = "";
  }
  const size_t len = std::strlen(src);
  const size_t n = (len < cap - 1) ? len : (cap - 1);
  std::memcpy(dst, src, n);
  dst[n] = '\0';
}

int16_t clamp_int16(long value) {
  if (value < -32768L) {
    return -32768;
  }
  if (value > 32767L) {
    return 32767;
  }
  return static_cast<int16_t>(value);
}

bool is_hex_sha256(const char* text) {
  if (text == nullptr || std::strlen(text) != 64) {
    return false;
  }
  for (size_t i = 0; i < 64; ++i) {
    const char c = text[i];
    const bool digit = c >= '0' && c <= '9';
    const bool lower = c >= 'a' && c <= 'f';
    const bool upper = c >= 'A' && c <= 'F';
    if (!digit && !lower && !upper) {
      return false;
    }
  }
  return true;
}

bool parse_version_parts(const char* text, uint32_t* parts, size_t& count) {
  count = 0;
  if (text == nullptr || *text == '\0') {
    return false;
  }

  const char* p = text;
  while (*p != '\0' && *p != '-' && *p != '+') {
    if (*p < '0' || *p > '9') {
      return false;
    }

    uint64_t value = 0;
    while (*p >= '0' && *p <= '9') {
      value = value * 10 + static_cast<uint64_t>(*p - '0');
      if (value > std::numeric_limits<uint32_t>::max()) {
        return false;
      }
      ++p;
    }
    if (count >= kMaxVersionParts) {
      return false;
    }
    parts[count++] = static_cast<uint32_t>(value);

    if (*p == '.') {
      ++p;
      if (*p < '0' || *p > '9') {
        return false;
      }
    } else {
      break;
    }
  }
  return count > 0;
}

}  // namespace

bool parse_manifest(const char* json, size_t len, ManifestInfo& out) {
  JsonDoc doc;
  if (!doc.parse(json, len)) {
    return false;
  }

  ManifestInfo manifest;
  if (!doc.get_string("version", manifest.version, sizeof(manifest.version)) ||
      manifest.version[0] == '\0') {
    return false;
  }
  if (!doc.get_string("bin_url", manifest.bin_url, sizeof(manifest.bin_url)) ||
      std::strncmp(manifest.bin_url, "https://", 8) != 0) {
    return false;
  }
  if (!doc.get_string("sha256", manifest.sha256, sizeof(manifest.sha256)) ||
      !is_hex_sha256(manifest.sha256)) {
    return false;
  }

  uint32_t size = 0;
  if (doc.get_uint("size", size)) {
    manifest.size = size;
  }

  out = manifest;
  return true;
}

bool version_is_newer(const char* candidate, const char* current) {
  uint32_t candidate_parts[kMaxVersionParts] = {};
  uint32_t current_parts[kMaxVersionParts] = {};
  size_t candidate_count = 0;
  size_t current_count = 0;

  if (!parse_version_parts(candidate, candidate_parts, candidate_count) ||
      !parse_version_parts(current, current_parts, current_count)) {
    return false;
  }

  const size_t count = candidate_count > current_count ? candidate_count : current_count;
  for (size_t i = 0; i < count; ++i) {
    const uint32_t candidate_part = i < candidate_count ? candidate_parts[i] : 0;
    const uint32_t current_part = i < current_count ? current_parts[i] : 0;
    if (candidate_part != current_part) {
      return candidate_part > current_part;
    }
  }
  return false;
}

bool parse_openmeteo_weather(const char* json, size_t len, WeatherInfo& out) {
  JsonDoc doc;
  if (!doc.parse(json, len)) {
    return false;
  }

  long temperature = 0;
  long apparent = 0;
  long weather_code = 0;
  if (!doc.get_int("current.temperature_2m", temperature) ||
      !doc.get_int("current.apparent_temperature", apparent) ||
      !doc.get_int("current.weather_code", weather_code)) {
    return false;
  }

  WeatherInfo weather = out;
  weather.temp_c = clamp_int16(temperature);
  weather.feels_like_c = clamp_int16(apparent);
  copy_text(weather.condition, sizeof(weather.condition),
            wmo_condition_ru(static_cast<int>(weather_code)));
  weather.valid = true;

  out = weather;
  return true;
}

bool parse_openmeteo_air(const char* json, size_t len, WeatherInfo& out) {
  JsonDoc doc;
  if (!doc.parse(json, len)) {
    return false;
  }

  long pm25 = 0;
  long aqi = 0;
  if (!doc.get_int("current.pm2_5", pm25) || !doc.get_int("current.us_aqi", aqi)) {
    return false;
  }

  WeatherInfo weather = out;
  weather.pm25 = clamp_int16(pm25);
  weather.aqi = clamp_int16(aqi);
  weather.valid = true;

  out = weather;
  return true;
}

const char* wmo_condition_ru(int code) {
  switch (code) {
    case 0:
      return "Ясно";
    case 1:
      return "Малооблачно";
    case 2:
      return "Переменно";
    case 3:
      return "Облачно";
    case 45:
    case 48:
      return "Туман";
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
      return "Морось";
    case 61:
    case 63:
    case 65:
      return "Дождь";
    case 66:
    case 67:
      return "Лед. дождь";
    case 71:
    case 73:
    case 75:
      return "Снег";
    case 77:
      return "Крупа";
    case 80:
    case 81:
    case 82:
      return "Ливень";
    case 85:
    case 86:
      return "Снегопад";
    case 95:
      return "Гроза";
    case 96:
    case 99:
      return "Гроза, град";
    default:
      return "Нет данных";
  }
}

}  // namespace tablo
