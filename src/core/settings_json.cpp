#include "settings_json.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/json.h"

namespace tablo {
namespace {

constexpr const char kExampleJson[] = R"json({
  "_instructions": "Заполните поля и сохраните файл как tablo-config.json. После импорта удалите пароли с карты.",
  "config_version": 1,
  "wifi_ssid": "",
  "wifi_password": "",
  "admin_user": "",
  "admin_password": "",
  "timezone": "MSK-3",
  "weather_lat": "",
  "weather_lon": "",
  "ping_gw": "",
  "ping_vps": "",
  "ping_dns": "",
  "ota_manifest_url": "https://shabbybastard.github.io/tablo/manifest.json",
  "note": "",
  "brightness": 80,
  "night_mode": false,
  "auto_ota": true,
  "weather_enabled": true,
  "date_format": 0,
  "night_schedule_enabled": false,
  "evening_start": "18:00",
  "night_start": "23:00",
  "day_start": "07:00"
}
)json";

void set_error(char* error, size_t cap, const char* text) {
  if (error == nullptr || cap == 0) {
    return;
  }
  std::snprintf(error, cap, "%s", text != nullptr ? text : "");
}

void set_field_error(char* error, size_t cap, const char* field, const char* text) {
  if (error == nullptr || cap == 0) {
    return;
  }
  std::snprintf(error, cap, "%s: %s", field, text);
}

bool has_control_chars(const char* text) {
  if (text == nullptr) {
    return false;
  }
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != '\0';
       ++p) {
    if (*p < 0x20 || *p == 0x7F) {
      return true;
    }
  }
  return false;
}

bool read_string_field(JsonDoc& doc, const char* key, char* dst, size_t cap,
                       char* error, size_t error_cap) {
  if (!doc.has(key)) {
    return true;
  }
  const int length = doc.get_string_length(key);
  if (length < 0) {
    set_field_error(error, error_cap, key, "ожидается строка");
    return false;
  }
  if (static_cast<size_t>(length) >= cap) {
    char text[64];
    std::snprintf(text, sizeof(text), "слишком длинное значение (макс. %u)",
                  static_cast<unsigned>(cap - 1));
    set_field_error(error, error_cap, key, text);
    return false;
  }
  if (!doc.get_string(key, dst, cap)) {
    set_field_error(error, error_cap, key, "не удалось прочитать строку");
    return false;
  }
  if (std::strlen(dst) != static_cast<size_t>(length) || has_control_chars(dst)) {
    set_field_error(error, error_cap, key, "управляющие символы запрещены");
    return false;
  }
  return true;
}

bool read_bool_field(JsonDoc& doc, const char* key, bool& value, char* error,
                     size_t error_cap) {
  if (!doc.has(key)) {
    return true;
  }
  if (!doc.get_bool(key, value)) {
    set_field_error(error, error_cap, key, "ожидается true или false");
    return false;
  }
  return true;
}

bool read_int_field(JsonDoc& doc, const char* key, long& value, char* error,
                    size_t error_cap, bool exact = false) {
  if (!doc.has(key)) {
    return true;
  }
  const bool ok = exact ? doc.get_int_exact(key, value) : doc.get_int(key, value);
  if (!ok) {
    set_field_error(error, error_cap, key, "ожидается целое число");
    return false;
  }
  return true;
}

bool starts_with(const char* text, const char* prefix) {
  return text != nullptr && std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

bool parse_coordinate(const char* text, double min, double max) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const double value = std::strtod(text, &end);
  return errno != ERANGE && end != text && *end == '\0' && std::isfinite(value) &&
         value >= min && value <= max;
}

}  // namespace

const char* settings_example_json() { return kExampleJson; }

uint32_t settings_config_hash(const char* data, size_t len) {
  uint32_t hash = 2166136261u;
  if (data == nullptr) {
    return hash;
  }
  for (size_t i = 0; i < len; ++i) {
    hash ^= static_cast<unsigned char>(data[i]);
    hash *= 16777619u;
  }
  return hash;
}

bool parse_settings_json(const char* json, size_t len, Settings& out, char* error,
                         size_t error_cap) {
  if (error != nullptr && error_cap > 0) {
    error[0] = '\0';
  }
  if (json == nullptr || len == 0) {
    set_error(error, error_cap, "Файл пустой");
    return false;
  }
  if (len > kSettingsJsonMaxBytes) {
    set_error(error, error_cap, "Файл слишком большой (максимум 8 КиБ)");
    return false;
  }

  /* Editors on Windows may add a UTF-8 BOM. jsmn does not accept it. */
  const char* text = json;
  size_t text_len = len;
  if (text_len >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text += 3;
    text_len -= 3;
  }

  JsonDoc doc;
  if (!doc.parse(text, text_len)) {
    set_error(error, error_cap, "Файл не является корректным JSON");
    return false;
  }

  if (doc.has("config_version")) {
    long version = 0;
    if (!doc.get_int_exact("config_version", version) || version != 1) {
      set_error(error, error_cap, "config_version: поддерживается только версия 1");
      return false;
    }
  }

  Settings settings = default_settings();

  struct StringField {
    const char* key;
    char* dst;
    size_t cap;
  };
  const StringField fields[] = {
      {"wifi_ssid", settings.wifi_ssid, sizeof(settings.wifi_ssid)},
      {"wifi_password", settings.wifi_password, sizeof(settings.wifi_password)},
      {"admin_user", settings.admin_user, sizeof(settings.admin_user)},
      {"admin_password", settings.admin_password, sizeof(settings.admin_password)},
      {"timezone", settings.timezone, sizeof(settings.timezone)},
      {"weather_lat", settings.weather_lat, sizeof(settings.weather_lat)},
      {"weather_lon", settings.weather_lon, sizeof(settings.weather_lon)},
      {"ping_gw", settings.ping_gw, sizeof(settings.ping_gw)},
      {"ping_vps", settings.ping_vps, sizeof(settings.ping_vps)},
      {"ping_dns", settings.ping_dns, sizeof(settings.ping_dns)},
      {"ota_manifest_url", settings.ota_manifest_url,
       sizeof(settings.ota_manifest_url)},
      {"note", settings.note, sizeof(settings.note)},
  };
  for (const StringField& field : fields) {
    if (!read_string_field(doc, field.key, field.dst, field.cap, error, error_cap)) {
      return false;
    }
  }

  long number = 0;
  if (doc.has("brightness")) {
    if (!read_int_field(doc, "brightness", number, error, error_cap, true)) {
      return false;
    }
    if (number < 10 || number > 100) {
      set_field_error(error, error_cap, "brightness", "допустимо 10..100");
      return false;
    }
    settings.brightness = static_cast<uint8_t>(number);
  }

  if (!read_bool_field(doc, "night_mode", settings.night_mode, error, error_cap)) {
    return false;
  }
  if (!read_bool_field(doc, "auto_ota", settings.auto_ota, error, error_cap)) {
    return false;
  }
  if (!read_bool_field(doc, "weather_enabled", settings.weather_enabled, error,
                       error_cap)) {
    return false;
  }
  if (doc.has("date_format")) {
    if (!read_int_field(doc, "date_format", number, error, error_cap, true)) {
      return false;
    }
    if (number < 0 || number > 2) {
      set_field_error(error, error_cap, "date_format", "допустимо 0..2");
      return false;
    }
    settings.date_format = static_cast<DateFormat>(number);
  }
  if (!read_bool_field(doc, "night_schedule_enabled", settings.schedule.enabled,
                       error, error_cap)) {
    return false;
  }

  bool schedule_time_present = false;
  const struct TimeField {
    const char* key;
    uint16_t* dst;
  } time_fields[] = {
      {"evening_start", &settings.schedule.evening_min},
      {"night_start", &settings.schedule.night_min},
      {"day_start", &settings.schedule.day_min},
  };
  for (const TimeField& field : time_fields) {
    if (!doc.has(field.key)) {
      continue;
    }
    schedule_time_present = true;
    const int length = doc.get_string_length(field.key);
    char text[12] = {};
    if (length < 0 || static_cast<size_t>(length) >= sizeof(text) ||
        !doc.get_string(field.key, text, sizeof(text)) ||
        !parse_hhmm(text, *field.dst)) {
      set_field_error(error, error_cap, field.key, "ожидается время HH:MM");
      return false;
    }
  }
  if ((settings.schedule.enabled || schedule_time_present) &&
      !night_schedule_valid(settings.schedule)) {
    set_error(error, error_cap,
              "Расписание: день, вечер и ночь должны идти по возрастанию");
    return false;
  }

  if (settings.wifi_ssid[0] == '\0') {
    set_field_error(error, error_cap, "wifi_ssid", "обязательное поле");
    return false;
  }
  if (settings.admin_user[0] == '\0') {
    set_field_error(error, error_cap, "admin_user", "обязательное поле");
    return false;
  }
  if (std::strlen(settings.admin_password) < 8 ||
      std::strcmp(settings.admin_password, "change-me") == 0) {
    set_field_error(error, error_cap, "admin_password",
                    "минимум 8 символов, не change-me");
    return false;
  }

  const bool lat_empty = settings.weather_lat[0] == '\0';
  const bool lon_empty = settings.weather_lon[0] == '\0';
  if (lat_empty != lon_empty) {
    set_error(error, error_cap,
              "weather_lat и weather_lon должны быть заполнены вместе");
    return false;
  }
  if (!lat_empty && !parse_coordinate(settings.weather_lat, -90.0, 90.0)) {
    set_field_error(error, error_cap, "weather_lat", "ожидается число от -90 до 90");
    return false;
  }
  if (!lon_empty && !parse_coordinate(settings.weather_lon, -180.0, 180.0)) {
    set_field_error(error, error_cap, "weather_lon", "ожидается число от -180 до 180");
    return false;
  }

  if (settings.ota_manifest_url[0] != '\0' &&
      !starts_with(settings.ota_manifest_url, "https://")) {
    set_field_error(error, error_cap, "ota_manifest_url", "разрешён только https://");
    return false;
  }

  out = settings;
  return true;
}

}  // namespace tablo
