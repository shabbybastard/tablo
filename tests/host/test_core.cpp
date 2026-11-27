#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/date_format.h"
#include "core/json.h"
#include "core/miio.h"
#include "core/parsers.h"
#include "core/settings.h"
#include "core/schedule.h"
#include "core/settings_json.h"
#include "core/settings_validate.h"
#include "core/state_store.h"

namespace {

using tablo::AppState;
using tablo::JsonDoc;
using tablo::ManifestInfo;
using tablo::Settings;
using tablo::WeatherInfo;
using tablo::g_state;

template <size_t N>
void assert_terminated(const char (&text)[N]) {
  assert(std::memchr(text, '\0', N) != nullptr);
}

void test_json_paths() {
  const char* json = R"json({
    "name": "tablo",
    "count": 42,
    "limit": 4294967295,
    "ratio": 2.6,
    "enabled": true,
    "weather": { "temp_c": -3, "condition": "Облачно" },
    "items": [
      { "name": "a", "value": 1, "ok": true },
      { "name": "b", "value": 2, "ok": false }
    ],
    "nums": [10, 20, 30],
    "tags": ["one", "two"],
    "escaped": "quote \" backslash \\ slash \/ tab\t unicode \u00e9 smile \uD83D\uDE00"
  })json";

  JsonDoc doc;
  assert(doc.parse(json, std::strlen(json)));

  char text[64];
  assert(doc.get_string("name", text, sizeof(text)));
  assert(std::strcmp(text, "tablo") == 0);

  long number = 0;
  assert(doc.get_int("count", number));
  assert(number == 42);
  assert(doc.get_int("ratio", number));
  assert(number == 3);
  assert(doc.get_int("weather.temp_c", number));
  assert(number == -3);
  assert(doc.get_int_exact("count", number));
  assert(number == 42);
  assert(!doc.get_int_exact("ratio", number));
  assert(doc.get_int_exact("weather.temp_c", number));
  assert(number == -3);
  assert(!doc.get_int_exact("enabled", number));

  uint32_t limit = 0;
  assert(doc.get_uint("limit", limit));
  assert(limit == UINT32_MAX);
  assert(!doc.get_uint("weather.temp_c", limit));

  bool flag = false;
  assert(doc.get_bool("enabled", flag));
  assert(flag);
  assert(!doc.get_bool("count", flag));

  assert(doc.get_string("weather.condition", text, sizeof(text)));
  assert(std::strcmp(text, "Облачно") == 0);

  assert(doc.array_size("items") == 2);
  assert(doc.array_size("nums") == 3);
  assert(doc.array_size("tags") == 2);
  assert(doc.array_size("name") == -1);
  assert(doc.array_size("missing") == -1);
  assert(doc.array_size("") == -1);

  assert(doc.get_string_at("items", 0, "name", text, sizeof(text)));
  assert(std::strcmp(text, "a") == 0);
  assert(doc.get_string_at("items", 1, "name", text, sizeof(text)));
  assert(std::strcmp(text, "b") == 0);

  assert(doc.get_int_at("items", 0, "value", number));
  assert(number == 1);
  assert(doc.get_int_at("items", 1, "value", number));
  assert(number == 2);

  assert(doc.get_bool_at("items", 0, "ok", flag));
  assert(flag);
  assert(doc.get_bool_at("items", 1, "ok", flag));
  assert(!flag);

  assert(doc.get_int_at("nums", 1, nullptr, number));
  assert(number == 20);
  assert(doc.get_int_at("nums", 2, nullptr, number));
  assert(number == 30);

  assert(doc.get_string_at("tags", 1, nullptr, text, sizeof(text)));
  assert(std::strcmp(text, "two") == 0);

  assert(!doc.get_string_at("items", 0, nullptr, text, sizeof(text)));
  assert(!doc.get_int_at("items", 0, "name", number));
  assert(!doc.get_int_at("items", 5, "value", number));
  assert(!doc.get_int_at("items", -1, "value", number));
  assert(!doc.get_int_at("items", 0, "missing", number));
  assert(!doc.get_string_at("missing", 0, nullptr, text, sizeof(text)));

  char tiny[4];
  assert(doc.get_string("name", tiny, sizeof(tiny)));
  assert(std::strcmp(tiny, "tab") == 0);

  const char* expected_escaped =
      "quote \" backslash \\ slash / tab\t unicode \xC3\xA9 smile \xF0\x9F\x98\x80";
  assert(doc.get_string("escaped", text, sizeof(text)));
  assert(std::strcmp(text, expected_escaped) == 0);

  assert(!doc.get_string("missing", text, sizeof(text)));
  assert(!doc.get_string("weather.temp_c", text, sizeof(text)));
  assert(!doc.get_int("name", number));
  assert(!doc.get_bool("weather.temp_c", flag));
  assert(!doc.get_string("name", text, 0));

  assert(!doc.parse("{", 1));
  assert(!doc.parse("", 0));
  assert(!doc.parse(nullptr, 0));
  assert(!doc.parse("not json", 8));

  const char* root_array = R"json([1, 2, 3])json";
  JsonDoc array_doc;
  assert(array_doc.parse(root_array, std::strlen(root_array)));
  assert(array_doc.array_size("") == 3);
  assert(array_doc.get_int_at("", 2, nullptr, number));
  assert(number == 3);
}

void test_manifest() {
  const char* json =
      R"json({"version":"1.2.3","bin_url":"https://example.com/fw.bin","sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","size":1234567})json";

  ManifestInfo manifest;
  assert(tablo::parse_manifest(json, std::strlen(json), manifest));
  assert(std::strcmp(manifest.version, "1.2.3") == 0);
  assert(std::strcmp(manifest.bin_url, "https://example.com/fw.bin") == 0);
  assert(std::strcmp(manifest.sha256,
                     "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") ==
         0);
  assert(manifest.size == 1234567u);

  const char* no_size =
      R"json({"version":"2.0.0","bin_url":"https://example.com/fw.bin","sha256":"ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})json";
  ManifestInfo no_size_manifest;
  assert(tablo::parse_manifest(no_size, std::strlen(no_size), no_size_manifest));
  assert(no_size_manifest.size == 0);

  const char* http_url =
      R"json({"version":"1.0.0","bin_url":"http://example.com/fw.bin","sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"})json";
  ManifestInfo unused;
  assert(!tablo::parse_manifest(http_url, std::strlen(http_url), unused));

  const char* short_sha =
      R"json({"version":"1.0.0","bin_url":"https://example.com/fw.bin","sha256":"abc"})json";
  assert(!tablo::parse_manifest(short_sha, std::strlen(short_sha), unused));

  const char* bad_hex =
      R"json({"version":"1.0.0","bin_url":"https://example.com/fw.bin","sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg"})json";
  assert(!tablo::parse_manifest(bad_hex, std::strlen(bad_hex), unused));

  const char* missing_version =
      R"json({"bin_url":"https://example.com/fw.bin","sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"})json";
  assert(!tablo::parse_manifest(missing_version, std::strlen(missing_version), unused));

  const char* invalid = "{";
  assert(!tablo::parse_manifest(invalid, std::strlen(invalid), unused));
}

void test_version_compare() {
  assert(tablo::version_is_newer("1.2.4", "1.2.3"));
  assert(tablo::version_is_newer("1.10.0", "1.9.9"));
  assert(tablo::version_is_newer("2.0", "1.9.9"));
  assert(tablo::version_is_newer("1.2.3.1", "1.2.3"));
  assert(tablo::version_is_newer("1.2.3-dev", "1.2.2"));
  assert(!tablo::version_is_newer("1.2.3", "1.2.3"));
  assert(!tablo::version_is_newer("1.2.2", "1.2.3"));
  assert(!tablo::version_is_newer("1.2.3-dev", "1.2.3"));
  assert(!tablo::version_is_newer("1.2.3", "1.2.3-dev"));
  assert(!tablo::version_is_newer("", "1.0.0"));
  assert(!tablo::version_is_newer("1.0.0", ""));
  assert(!tablo::version_is_newer("garbage", "1.0.0"));
}

void test_weather() {
  WeatherInfo weather;
  weather.pm25 = 7;
  weather.aqi = 33;
  std::strcpy(weather.condition, "old");

  const char* json =
      R"json({"current":{"temperature_2m":-3.6,"apparent_temperature":-7.5,"weather_code":3}})json";
  assert(tablo::parse_openmeteo_weather(json, std::strlen(json), weather));
  assert(weather.valid);
  assert(weather.temp_c == -4);
  assert(weather.feels_like_c == -8);
  assert(std::strcmp(weather.condition, "Облачно") == 0);
  assert(weather.pm25 == 7);
  assert(weather.aqi == 33);

  const char* null_temp =
      R"json({"current":{"temperature_2m":null,"apparent_temperature":-7.5,"weather_code":3}})json";
  WeatherInfo unchanged;
  unchanged.temp_c = 5;
  assert(!tablo::parse_openmeteo_weather(null_temp, std::strlen(null_temp), unchanged));
  assert(unchanged.temp_c == 5);
  assert(!unchanged.valid);

  assert(std::strcmp(tablo::wmo_condition_ru(0), "Ясно") == 0);
  assert(std::strcmp(tablo::wmo_condition_ru(3), "Облачно") == 0);
  assert(std::strcmp(tablo::wmo_condition_ru(45), "Туман") == 0);
  assert(std::strcmp(tablo::wmo_condition_ru(61), "Дождь") == 0);
  assert(std::strcmp(tablo::wmo_condition_ru(71), "Снег") == 0);
  assert(std::strcmp(tablo::wmo_condition_ru(95), "Гроза") == 0);
  assert(std::strcmp(tablo::wmo_condition_ru(-1), "Нет данных") == 0);
}

void test_air() {
  WeatherInfo weather;
  weather.valid = true;
  weather.temp_c = -5;
  weather.feels_like_c = -8;
  std::strcpy(weather.condition, "Облачно");

  const char* json = R"json({"current":{"pm2_5":12.6,"us_aqi":48.4}})json";
  assert(tablo::parse_openmeteo_air(json, std::strlen(json), weather));
  assert(weather.valid);
  assert(weather.pm25 == 13);
  assert(weather.aqi == 48);
  assert(weather.temp_c == -5);
  assert(weather.feels_like_c == -8);
  assert(std::strcmp(weather.condition, "Облачно") == 0);

  WeatherInfo unchanged;
  unchanged.pm25 = 4;
  unchanged.aqi = 9;
  const char* bad = R"json({"current":{"pm2_5":null,"us_aqi":48}})json";
  assert(!tablo::parse_openmeteo_air(bad, std::strlen(bad), unchanged));
  assert(unchanged.pm25 == 4);
  assert(unchanged.aqi == 9);
}

void test_state_store() {
  AppState first;
  first.wifi_rssi = -40;
  first.revision = 10;
  g_state.set(first);

  AppState snapshot = g_state.snapshot();
  assert(snapshot.revision == 11);
  assert(snapshot.wifi_rssi == -40);

  g_state.update_wifi(true, -52, "192.0.2.42");
  snapshot = g_state.snapshot();
  assert(snapshot.revision == 12);
  assert(snapshot.wifi_connected);
  assert(snapshot.wifi_rssi == -52);
  assert(std::strcmp(snapshot.ip, "192.0.2.42") == 0);

  g_state.update_wifi(false, -90, nullptr);
  snapshot = g_state.snapshot();
  assert(snapshot.revision == 13);
  assert(!snapshot.wifi_connected);
  assert(snapshot.ip[0] == '\0');

  g_state.update_wifi(true, -50, "12345678901234567890");
  snapshot = g_state.snapshot();
  assert(snapshot.revision == 14);
  assert(std::strlen(snapshot.ip) == sizeof(snapshot.ip) - 1);
  assert(std::strcmp(snapshot.ip, "123456789012345") == 0);
}

void test_settings() {
  const Settings settings = tablo::default_settings();

  assert(settings.brightness == 80);
  assert(!settings.night_mode);
  assert(settings.auto_ota);
  assert(!settings.schedule.enabled);
  assert(tablo::night_schedule_valid(settings.schedule));
  assert(std::strcmp(settings.timezone, "MSK-3") == 0);
  assert(std::strcmp(settings.note, "Все системы в норме") == 0);

  assert_terminated(settings.wifi_ssid);
  assert_terminated(settings.wifi_password);
  assert_terminated(settings.admin_user);
  assert_terminated(settings.admin_password);
  assert_terminated(settings.timezone);
  assert_terminated(settings.weather_lat);
  assert_terminated(settings.weather_lon);
  assert_terminated(settings.ping_gw);
  assert_terminated(settings.ping_vps);
  assert_terminated(settings.ping_dns);
  assert_terminated(settings.ota_manifest_url);
  assert_terminated(settings.note);
}

void test_json_exact_lengths() {
  const char* json = R"json({"s":"hello","e":"a\u00e9b","long":"1234567890"})json";
  JsonDoc doc;
  assert(doc.parse(json, std::strlen(json)));
  assert(doc.has("s"));
  assert(doc.has("e"));
  assert(!doc.has("missing"));
  assert(doc.get_string_length("s") == 5);
  assert(doc.get_string_length("e") == 4);  // a + U+00E9 (2 bytes) + b
  assert(doc.get_string_length("long") == 10);
  assert(doc.get_string_length("missing") == -1);
  assert(doc.get_string_length("s") == static_cast<int>(std::strlen("hello")));

  char tiny[4];
  assert(doc.get_string("long", tiny, sizeof(tiny)));
  assert(std::strcmp(tiny, "123") == 0);
  assert(doc.get_string_length("long") == 10);
}

void test_miio() {
  uint8_t token[tablo::kMiioTokenBytes] = {};
  assert(tablo::miio_parse_token("0123456789abcdef0123456789ABCDEF", token));
  assert(token[0] == 0x01 && token[15] == 0xEF);
  assert(!tablo::miio_parse_token("0123456789abcdef", token));
  assert(!tablo::miio_parse_token("0123456789abcdef0123456789abcdeZ", token));
  assert(!tablo::miio_parse_token("", token));

  assert(tablo::miio_valid_ipv4("192.0.2.50"));
  assert(tablo::miio_valid_ipv4("0.0.0.0"));
  assert(tablo::miio_valid_ipv4("255.255.255.255"));
  assert(!tablo::miio_valid_ipv4("192.0.2"));
  assert(!tablo::miio_valid_ipv4("192.0.2.256"));
  assert(!tablo::miio_valid_ipv4("192.0.2.1 "));
  assert(!tablo::miio_valid_ipv4("purifier.local"));

  const tablo::MiioMapping* mb3 =
      tablo::miio_mapping_for_model("zhimi.airpurifier.mb3");
  assert(mb3 != nullptr && !mb3->legacy_get_prop);
  assert(mb3->power_siid == 2 && mb3->power_piid == 2);
  assert(mb3->fan_siid == 2 && mb3->fan_piid == 4);
  assert(mb3->aqi_siid == 3 && mb3->aqi_piid == 6);

  const tablo::MiioMapping* mb4 =
      tablo::miio_mapping_for_model("zhimi.airp.mb4a");
  assert(mb4 != nullptr && mb4->power_piid == 1 && mb4->aqi_piid == 4);
  assert(mb4->temperature_siid == 0);

  const tablo::MiioMapping* va2 =
      tablo::miio_mapping_for_model("zhimi.airp.mb5");
  assert(va2 != nullptr && va2->humidity_siid == 3 && va2->filter_piid == 1);

  const tablo::MiioMapping* legacy =
      tablo::miio_mapping_for_model("zhimi.airpurifier.mc1");
  assert(legacy != nullptr && legacy->legacy_get_prop);
  assert(tablo::miio_model_supported("zhimi.airpurifier.mc1"));
  assert(tablo::miio_model_supported("zhimi.airp.mb5"));
  assert(tablo::miio_model_supported("zhimi.airp.rma3"));
  assert(!tablo::miio_model_supported("some.unknown.model"));

  char request[1024];
  const size_t request_len =
      tablo::miio_build_get_properties(7, *va2, request, sizeof(request));
  assert(request_len > 0);
  assert(std::strstr(request, "\"method\":\"get_properties\"") != nullptr);
  assert(std::strstr(request, "\"did\":\"power\",\"siid\":2,\"piid\":1") !=
         nullptr);
  assert(std::strstr(request, "\"did\":\"humidity\",\"siid\":3,\"piid\":1") !=
         nullptr);
  assert(std::strstr(request, "\"did\":\"tvoc\"") == nullptr);

  char legacy_request[256];
  assert(tablo::miio_build_get_prop(8, legacy_request,
                                    sizeof(legacy_request)) > 0);
  assert(std::strstr(legacy_request, "\"method\":\"get_prop\"") != nullptr);

  const char* miot_response = R"json({
    "id": 7,
    "result": [
      {"did":"power","siid":2,"piid":1,"code":0,"value":true},
      {"did":"mode","siid":2,"piid":4,"code":0,"value":2},
      {"did":"fan_level","siid":2,"piid":5,"code":0,"value":3},
      {"did":"humidity","siid":3,"piid":1,"code":0,"value":45},
      {"did":"temperature","siid":3,"piid":7,"code":0,"value":22.6},
      {"did":"aqi","siid":3,"piid":4,"code":0,"value":12},
      {"did":"filter_life","siid":4,"piid":1,"code":0,"value":81},
      {"did":"buzzer","siid":6,"piid":1,"code":-4001}
    ]
  })json";
  tablo::MiioReading reading;
  assert(tablo::miio_parse_miot_properties(miot_response,
                                           std::strlen(miot_response),
                                           reading));
  assert(reading.has_power && reading.power);
  assert(reading.mode == 2 && reading.fan_level == 3);
  assert(reading.has_humidity && reading.humidity == 45);
  assert(reading.has_temperature && reading.temperature == 23);
  assert(reading.pm25 == 12 && reading.filter_life == 81);

  const char* legacy_response =
      R"json({"id":8,"result":["on","auto",18,46,234,64]})json";
  assert(tablo::miio_parse_legacy_props(legacy_response,
                                        std::strlen(legacy_response),
                                        reading));
  assert(reading.has_power && reading.power);
  assert(reading.mode == 0);
  assert(reading.pm25 == 18);
  assert(reading.humidity == 46);
  assert(reading.temperature == 23);
  assert(reading.filter_life == 64);

  const char* info =
      R"json({"id":9,"result":{"model":"zhimi.airp.mb5","fw_ver":"1.0"}})json";
  char model[32] = {};
  assert(tablo::miio_parse_model(info, std::strlen(info), model,
                                 sizeof(model)));
  assert(std::strcmp(model, "zhimi.airp.mb5") == 0);

  assert(std::strcmp(tablo::miio_mode_text(0), "Авто") == 0);
  assert(std::strcmp(tablo::miio_quality_text(8), "Чисто") == 0);
  assert(std::strcmp(tablo::miio_quality_text(200), "Опасно") == 0);
}

void test_settings_json() {
  const char* json = R"json({
    "config_version": 1,
    "wifi_ssid": "test-net",
    "wifi_password": "test-pass-123",
    "admin_user": "admin",
    "admin_password": "strong-pass-123",
    "timezone": "MSK-3",
    "weather_lat": "55.75",
    "weather_lon": "37.61",
    "ping_gw": "192.0.2.1",
    "ping_vps": "198.51.100.10",
    "ping_dns": "203.0.113.53",
    "ota_manifest_url": "https://example.com/manifest.json",
    "note": "test note",
    "brightness": 42,
    "night_mode": true,
    "auto_ota": false,
    "weather_enabled": false,
    "date_format": 2,
    "night_schedule_enabled": true,
    "evening_start": "18:00",
    "night_start": "23:00",
    "day_start": "07:00",
    "unknown": "ignored"
  })json";

  Settings out;
  char error[192] = {};
  assert(tablo::parse_settings_json(json, std::strlen(json), out, error, sizeof(error)));
  assert(std::strcmp(out.wifi_ssid, "test-net") == 0);
  assert(std::strcmp(out.wifi_password, "test-pass-123") == 0);
  assert(std::strcmp(out.admin_user, "admin") == 0);
  assert(std::strcmp(out.admin_password, "strong-pass-123") == 0);
  assert(std::strcmp(out.weather_lat, "55.75") == 0);
  assert(std::strcmp(out.weather_lon, "37.61") == 0);
  assert(std::strcmp(out.ping_dns, "203.0.113.53") == 0);
  assert(out.brightness == 42);
  assert(out.night_mode);
  assert(!out.auto_ota);
  assert(!out.weather_enabled);
  assert(out.date_format == tablo::DateFormat::Numeric);
  assert(out.schedule.enabled);
  assert(out.schedule.evening_min == 18U * 60U);
  assert(out.schedule.night_min == 23U * 60U);
  assert(out.schedule.day_min == 7U * 60U);
  assert(!tablo::settings_need_setup(out));

  /* The empty template is not applyable: required fields must be filled. */
  const char* example = tablo::settings_example_json();
  assert(example != nullptr);
  assert(std::strstr(example, "\"brightness\"") != nullptr);
  assert(std::strstr(example, "\"purifier_enabled\"") != nullptr);
  assert(std::strstr(example, "\"purifier_token\"") != nullptr);
  assert(std::strstr(example, "\"tablo-config.example.json\"") == nullptr);
  Settings template_out;
  error[0] = '\0';
  assert(!tablo::parse_settings_json(example, std::strlen(example), template_out, error,
                                    sizeof(error)));
  assert(std::strstr(error, "wifi_ssid") != nullptr);

  /* UTF-8 BOM and CRLF are accepted. */
  std::string bom = "\xEF\xBB\xBF";
  bom += json;
  Settings bom_out;
  assert(tablo::parse_settings_json(bom.c_str(), bom.size(), bom_out, error,
                                   sizeof(error)));

  /* Missing optional keys keep defaults. */
  const char* minimal =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123"})json";
  Settings minimal_out;
  assert(tablo::parse_settings_json(minimal, std::strlen(minimal), minimal_out, error,
                                    sizeof(error)));
  assert(std::strcmp(minimal_out.timezone, "MSK-3") == 0);
  assert(minimal_out.brightness == 80);
  assert(minimal_out.auto_ota);

  /* Oversized values are rejected, not silently truncated. */
  const char* long_user =
      R"json({"wifi_ssid":"net","admin_user":"1234567890123456789012345678901234567890","admin_password":"strong-pass-123"})json";
  assert(!tablo::parse_settings_json(long_user, std::strlen(long_user), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "admin_user") != nullptr);

  const char* bad_type =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","brightness":"80"})json";
  assert(!tablo::parse_settings_json(bad_type, std::strlen(bad_type), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "brightness") != nullptr);

  const char* bad_brightness =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","brightness":5})json";
  assert(!tablo::parse_settings_json(bad_brightness, std::strlen(bad_brightness), out,
                                     error, sizeof(error)));

  const char* bad_version =
      R"json({"config_version":2,"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123"})json";
  assert(!tablo::parse_settings_json(bad_version, std::strlen(bad_version), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "config_version") != nullptr);

  const char* float_version =
      R"json({"config_version":1.4,"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123"})json";
  assert(!tablo::parse_settings_json(float_version, std::strlen(float_version), out,
                                     error, sizeof(error)));
  assert(std::strstr(error, "config_version") != nullptr);

  const char* fractional_brightness =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","brightness":80.5})json";
  assert(!tablo::parse_settings_json(fractional_brightness,
                                     std::strlen(fractional_brightness), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "brightness") != nullptr);

  const char* half_weather =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","weather_lat":"55.75"})json";
  assert(!tablo::parse_settings_json(half_weather, std::strlen(half_weather), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "weather_lon") != nullptr);

  const char* http_ota =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","ota_manifest_url":"http://example.com/manifest.json"})json";
  assert(!tablo::parse_settings_json(http_ota, std::strlen(http_ota), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "ota_manifest_url") != nullptr);

  const char* weak_admin =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"short"})json";
  assert(!tablo::parse_settings_json(weak_admin, std::strlen(weak_admin), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "admin_password") != nullptr);

  const char* bad_schedule =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","night_schedule_enabled":true,"evening_start":"23:00","night_start":"18:00","day_start":"07:00"})json";
  assert(!tablo::parse_settings_json(bad_schedule, std::strlen(bad_schedule), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "Расписание") != nullptr);

  const char* bad_schedule_time =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","evening_start":"25:00"})json";
  assert(!tablo::parse_settings_json(bad_schedule_time,
                                     std::strlen(bad_schedule_time), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "evening_start") != nullptr);

  const char* nul_in_string =
      R"json({"wifi_ssid":"net","admin_user":"adm\u0000in","admin_password":"strong-pass-123"})json";
  assert(!tablo::parse_settings_json(nul_in_string, std::strlen(nul_in_string), out,
                                     error, sizeof(error)));
  assert(std::strstr(error, "admin_user") != nullptr);

  assert(!tablo::parse_settings_json("{}", 2, out, error, sizeof(error)));
  assert(std::strstr(error, "wifi_ssid") != nullptr);
  assert(!tablo::parse_settings_json("", 0, out, error, sizeof(error)));

  const char* purifier =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","purifier_enabled":true,"purifier_ip":"192.0.2.50","purifier_token":"0123456789abcdef0123456789abcdef"})json";
  assert(tablo::parse_settings_json(purifier, std::strlen(purifier), out, error,
                                    sizeof(error)));
  assert(out.purifier_enabled);
  assert(std::strcmp(out.purifier_ip, "192.0.2.50") == 0);
  assert(std::strlen(out.purifier_token) == 32);

  const char* bad_token =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","purifier_token":"not-a-token"})json";
  assert(!tablo::parse_settings_json(bad_token, std::strlen(bad_token), out,
                                     error, sizeof(error)));
  assert(std::strstr(error, "purifier_token") != nullptr);

  const char* bad_ip =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","purifier_ip":"192.0.2.999"})json";
  assert(!tablo::parse_settings_json(bad_ip, std::strlen(bad_ip), out, error,
                                     sizeof(error)));
  assert(std::strstr(error, "purifier_ip") != nullptr);

  const char* enabled_without_token =
      R"json({"wifi_ssid":"net","admin_user":"admin","admin_password":"strong-pass-123","purifier_enabled":true,"purifier_ip":"192.0.2.50"})json";
  assert(!tablo::parse_settings_json(enabled_without_token,
                                     std::strlen(enabled_without_token), out,
                                     error, sizeof(error)));
  assert(std::strstr(error, "Очиститель") != nullptr);
}

void test_schedule() {
  uint16_t minutes = 0;
  assert(tablo::parse_hhmm("18:00", minutes));
  assert(minutes == 18U * 60U);
  assert(tablo::parse_hhmm("7:05", minutes));
  assert(minutes == 7U * 60U + 5U);
  assert(!tablo::parse_hhmm("", minutes));
  assert(!tablo::parse_hhmm("24:00", minutes));
  assert(!tablo::parse_hhmm("7", minutes));
  assert(!tablo::parse_hhmm("7:60", minutes));
  assert(!tablo::parse_hhmm("xx:yy", minutes));

  char text[8];
  tablo::format_hhmm(18U * 60U, text, sizeof(text));
  assert(std::strcmp(text, "18:00") == 0);
  tablo::format_hhmm(7U * 60U + 5U, text, sizeof(text));
  assert(std::strcmp(text, "07:05") == 0);

  tablo::NightSchedule schedule;
  assert(tablo::night_schedule_valid(schedule));
  schedule.enabled = true;

  assert(tablo::display_mode_for(schedule, 7U * 60U, true) ==
         tablo::DisplayMode::Day);
  assert(tablo::display_mode_for(schedule, 17U * 60U + 59U, true) ==
         tablo::DisplayMode::Day);
  assert(tablo::display_mode_for(schedule, 18U * 60U, true) ==
         tablo::DisplayMode::Evening);
  assert(tablo::display_mode_for(schedule, 22U * 60U + 59U, true) ==
         tablo::DisplayMode::Evening);
  assert(tablo::display_mode_for(schedule, 23U * 60U, true) ==
         tablo::DisplayMode::Night);
  assert(tablo::display_mode_for(schedule, 0, true) ==
         tablo::DisplayMode::Night);
  assert(tablo::display_mode_for(schedule, 6U * 60U + 59U, true) ==
         tablo::DisplayMode::Night);
  assert(tablo::display_mode_for(schedule, 7U * 60U, true) ==
         tablo::DisplayMode::Day);
  assert(tablo::display_mode_for(schedule, 18U * 60U, false) ==
         tablo::DisplayMode::Day);

  schedule.enabled = false;
  assert(tablo::display_mode_for(schedule, 23U * 60U, true) ==
         tablo::DisplayMode::Day);
  schedule.enabled = true;
  schedule.evening_min = schedule.night_min;
  assert(!tablo::night_schedule_valid(schedule));
  assert(tablo::display_mode_for(schedule, 23U * 60U, true) ==
         tablo::DisplayMode::Day);

  assert(tablo::brightness_for_mode(80, tablo::DisplayMode::Day) == 80);
  assert(tablo::brightness_for_mode(80, tablo::DisplayMode::Evening) == 48);
  assert(tablo::brightness_for_mode(80, tablo::DisplayMode::Night) == 20);
  assert(tablo::brightness_for_mode(10, tablo::DisplayMode::Evening) == 10);
  assert(tablo::brightness_for_mode(10, tablo::DisplayMode::Night) == 5);
}

void test_settings_hash_and_setup() {
  const char* a = "tablo-config-a";
  const char* b = "tablo-config-b";
  assert(tablo::settings_config_hash(a, std::strlen(a)) ==
         tablo::settings_config_hash(a, std::strlen(a)));
  assert(tablo::settings_config_hash(a, std::strlen(a)) !=
         tablo::settings_config_hash(b, std::strlen(b)));

  Settings settings;
  assert(tablo::settings_need_setup(settings));
  std::strcpy(settings.wifi_ssid, "net");
  assert(tablo::settings_need_setup(settings));
  std::strcpy(settings.admin_user, "admin");
  std::strcpy(settings.admin_password, "change-me");
  assert(tablo::settings_need_setup(settings));
  std::strcpy(settings.admin_password, "strong-pass-123");
  assert(!tablo::settings_need_setup(settings));
}

}  // namespace

void test_date_format() {
  char out[32] = {};
  tablo::format_date_text(out, sizeof(out), 26, 11, 2026, 4,
                          tablo::DateFormat::DayMonthWeekday);
  assert(std::strcmp(out, "26 ноя · Чт") == 0);
  tablo::format_date_text(out, sizeof(out), 26, 11, 2026, 4,
                          tablo::DateFormat::WeekdayDayMonth);
  assert(std::strcmp(out, "Чт, 26 ноя") == 0);
  tablo::format_date_text(out, sizeof(out), 26, 11, 2026, 4,
                          tablo::DateFormat::Numeric);
  assert(std::strcmp(out, "26.11.2026") == 0);
}

int main() {
  test_json_paths();
  test_json_exact_lengths();
  test_manifest();
  test_version_compare();
  test_weather();
  test_air();
  test_state_store();
  test_miio();
  test_settings();
  test_settings_json();
  test_schedule();
  test_date_format();
  test_settings_hash_and_setup();

  std::puts("test_core: all tests passed");
  return 0;
}
