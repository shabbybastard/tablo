#include "hal_wasm/mock_provider.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "core/json.h"
#include "core/miio.h"
#include "core/state_store.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Browser-local wall clock and the same short Russian date format the UI
// expects from the firmware provider.
EM_JS(void, tablo_preview_local_time, (int* hour, int* minute, char* date_buf, int date_cap), {
  var now = new Date();
  if (hour) HEAP32[hour >> 2] = now.getHours();
  if (minute) HEAP32[minute >> 2] = now.getMinutes();
  if (date_buf) {
    var days = [
      '\u0412\u0441', '\u041f\u043d', '\u0412\u0442', '\u0421\u0440',
      '\u0427\u0442', '\u041f\u0442', '\u0421\u0431'
    ];
    var months = [
      '\u044f\u043d\u0432', '\u0444\u0435\u0432', '\u043c\u0430\u0440',
      '\u0430\u043f\u0440', '\u043c\u0430\u044f', '\u0438\u044e\u043d',
      '\u0438\u044e\u043b', '\u0430\u0432\u0433', '\u0441\u0435\u043d',
      '\u043e\u043a\u0442', '\u043d\u043e\u044f', '\u0434\u0435\u043a'
    ];
    var text = now.getDate() + ' ' + months[now.getMonth()] + ' · ' + days[now.getDay()];
    var bytes = new TextEncoder().encode(text);
    if (bytes.length >= date_cap) bytes = bytes.slice(0, date_cap - 1);
    HEAPU8.set(bytes, date_buf);
    HEAPU8[date_buf + bytes.length] = 0;
  }
});
#endif

namespace tablo {
namespace {

template <typename T>
T clamp_value(long value, T low, T high) {
  if (value < static_cast<long>(low)) return low;
  if (value > static_cast<long>(high)) return high;
  return static_cast<T>(value);
}

long get_long(const JsonDoc& doc, const char* path, long fallback = 0) {
  long value = 0;
  return doc.get_int(path, value) ? value : fallback;
}

bool get_bool(const JsonDoc& doc, const char* path, bool fallback = false) {
  bool value = false;
  return doc.get_bool(path, value) ? value : fallback;
}

/** Copy a JSON string into a fixed-size field without ever overflowing it. */
void read_string(const JsonDoc& doc, const char* path, char* out, size_t cap) {
  if (out == nullptr || cap == 0) return;
  out[0] = '\0';
  doc.get_string(path, out, cap);
}

}  // namespace

void MockProvider::begin(const char* mock_path) {
  if (mock_path != nullptr && mock_path[0] != '\0') {
    std::snprintf(mock_path_, sizeof(mock_path_), "%s", mock_path);
  }
  if (!loaded_) {
    if (!load(mock_path_)) {
      applyFallback();
    }
    loaded_ = true;
  }
  updateClock();
  updateDisplayMode();
  g_state.set(state_);
}

bool MockProvider::refresh(AppState& state) {
  if (!loaded_) {
    if (!load(mock_path_)) {
      applyFallback();
    }
    loaded_ = true;
    updateClock();
  }

  ++tick_count_;
  updateClock();
  updateDisplayMode();
  processRequests();
  ++state_.revision;

  // The preview HAL publishes scan results and Wi-Fi status directly into
  // g_state; keep them instead of replacing the whole state with the mock.
  const WifiScanInfo scan = state.wifi_scan;
  char wifi_status[sizeof(state.wifi_status)] = {};
  std::snprintf(wifi_status, sizeof(wifi_status), "%s", state.wifi_status);

  state = state_;
  state.wifi_scan = scan;
  std::snprintf(state.wifi_status, sizeof(state.wifi_status), "%s",
                wifi_status);
  state.uptime_s = 3720U + tick_count_ / 4U;
  state.cpu_mhz = 240;
  state.flash_used = 1887184;
  state.flash_total = 6225152;
  state.ram_free = 182U * 1024U;
  state.ram_total = 320U * 1024U;
  return loaded_;
}

void MockProvider::requestUpdateCheck() {
  update_check_pending_ = true;
  std::snprintf(state_.ota.status, sizeof(state_.ota.status), "Проверка обновлений…");
}

void MockProvider::requestInstallUpdate() {
  install_pending_ = true;
}

void MockProvider::setBrightness(uint8_t percent) {
  state_.brightness = static_cast<uint8_t>(std::min<uint8_t>(percent, 100));
}

void MockProvider::setNightMode(bool on) {
  state_.night_mode = on;
}

void MockProvider::setSchedule(const NightSchedule& schedule) {
  state_.schedule = schedule;
  updateDisplayMode();
}

void MockProvider::setExternalConfig(bool enabled, const char* ip,
                                     const char* token) {
  ExternalInfo& external = state_.external;
  external.enabled = enabled;
  if (ip != nullptr) {
    std::snprintf(external.ip, sizeof(external.ip), "%s", ip);
  }
  if (token != nullptr && token[0] != '\0') {
    external.token_set = true;
  }
  external.configured =
      miio_valid_ipv4(external.ip) && external.token_set;

  if (!external.enabled) {
    external.valid = false;
    external.online = false;
    std::snprintf(external.status, sizeof(external.status), "Выключено");
  } else if (!external.configured) {
    external.valid = false;
    external.online = false;
    std::snprintf(external.status, sizeof(external.status), "Не настроено");
  } else {
    if (!external.valid) {
      external.valid = true;
      external.power = true;
      external.power_valid = true;
      external.mode = 0;
      external.fan_level = 2;
      external.pm25 = 8;
      external.temperature = 23;
      external.temperature_valid = true;
      external.humidity = 42;
      external.humidity_valid = true;
      external.filter_life = 76;
      std::snprintf(external.model, sizeof(external.model),
                    "zhimi.airpurifier.mb3");
      external.updated_uptime_s = 3720U + tick_count_ / 4U;
    }
    external.online = true;
    std::snprintf(external.status, sizeof(external.status), "Подключено");
  }
  g_state.set(state_);
}

void MockProvider::requestExternalRefresh() {
  ExternalInfo& external = state_.external;
  if (!external.enabled || !external.configured) return;
  external.valid = true;
  external.online = true;
  external.updated_uptime_s = 3720U + tick_count_ / 4U;
  std::snprintf(external.status, sizeof(external.status), "Подключено");
  g_state.set(state_);
}

bool MockProvider::load(const char* mock_path) {
  FILE* file = std::fopen(mock_path, "rb");
  if (file == nullptr) {
    std::fprintf(stderr, "[tablo] %s is not available\n", mock_path);
    return false;
  }

  std::fseek(file, 0, SEEK_END);
  const long file_size = std::ftell(file);
  std::rewind(file);
  if (file_size <= 0) {
    std::fclose(file);
    std::fprintf(stderr, "[tablo] %s is empty\n", mock_path);
    return false;
  }

  std::string text(static_cast<size_t>(file_size), '\0');
  const size_t read = std::fread(text.data(), 1, text.size(), file);
  std::fclose(file);
  if (read != text.size()) {
    std::fprintf(stderr, "[tablo] could not read %s\n", mock_path);
    return false;
  }

  JsonDoc doc;
  if (!doc.parse(text.c_str(), text.size())) {
    std::fprintf(stderr, "[tablo] could not parse %s\n", mock_path);
    return false;
  }

  state_ = AppState{};

  read_string(doc, "time.date_text", state_.time.date_text, sizeof(state_.time.date_text));
  state_.time.hour = static_cast<uint8_t>(clamp_value<long>(get_long(doc, "time.hour"), 0, 23));
  state_.time.minute = static_cast<uint8_t>(clamp_value<long>(get_long(doc, "time.minute"), 0, 59));
  state_.time.valid = true;

  state_.weather.valid = get_bool(doc, "weather.valid");
  state_.weather.temp_c =
      static_cast<int16_t>(clamp_value<long>(get_long(doc, "weather.temp_c"), -32768, 32767));
  state_.weather.feels_like_c =
      static_cast<int16_t>(clamp_value<long>(get_long(doc, "weather.feels_like_c"), -32768, 32767));
  state_.weather.pm25 =
      static_cast<int16_t>(clamp_value<long>(get_long(doc, "weather.pm25"), 0, 32767));
  state_.weather.aqi =
      static_cast<int16_t>(clamp_value<long>(get_long(doc, "weather.aqi"), 0, 32767));
  read_string(doc, "weather.condition", state_.weather.condition,
              sizeof(state_.weather.condition));

  ExternalInfo& external = state_.external;
  external.enabled = get_bool(doc, "external.enabled");
  external.configured = get_bool(doc, "external.configured");
  external.valid = get_bool(doc, "external.valid");
  external.online = get_bool(doc, "external.online");
  external.token_set = get_bool(doc, "external.token_set");
  read_string(doc, "external.ip", external.ip, sizeof(external.ip));
  read_string(doc, "external.model", external.model, sizeof(external.model));
  read_string(doc, "external.status", external.status, sizeof(external.status));
  external.power = get_bool(doc, "external.power");
  external.power_valid = get_bool(doc, "external.power_valid");
  external.mode = static_cast<int16_t>(
      clamp_value<long>(get_long(doc, "external.mode", -1), -1, 15));
  external.fan_level = static_cast<int16_t>(
      clamp_value<long>(get_long(doc, "external.fan_level", -1), -1, 15));
  external.pm25 = static_cast<int16_t>(
      clamp_value<long>(get_long(doc, "external.pm25", -1), -1, 1000));
  external.temperature = static_cast<int16_t>(
      clamp_value<long>(get_long(doc, "external.temperature"), -40, 80));
  external.temperature_valid = get_bool(doc, "external.temperature_valid");
  external.humidity = static_cast<int16_t>(
      clamp_value<long>(get_long(doc, "external.humidity"), 0, 100));
  external.humidity_valid = get_bool(doc, "external.humidity_valid");
  external.filter_life = static_cast<int16_t>(
      clamp_value<long>(get_long(doc, "external.filter_life", -1), -1, 100));
  external.tvoc = static_cast<int16_t>(
      clamp_value<long>(get_long(doc, "external.tvoc", -1), -1, 10000));
  external.updated_uptime_s = static_cast<uint32_t>(
      clamp_value<long>(get_long(doc, "external.updated_uptime_s"), 0,
                        2147483647L));

  for (size_t i = 0; i < kMaxPingTargets; ++i) {
    state_.pings[i] = PingInfo{};
  }
  const int ping_count = doc.array_size("pings");
  for (int i = 0; i < ping_count && i < static_cast<int>(kMaxPingTargets); ++i) {
    PingInfo& ping = state_.pings[i];
    char label[8] = "";
    if (doc.get_string_at("pings", i, "label", label, sizeof(label))) {
      std::snprintf(ping.label, sizeof(ping.label), "%s", label);
    }
    bool flag = false;
    if (doc.get_bool_at("pings", i, "configured", flag)) ping.configured = flag;
    if (doc.get_bool_at("pings", i, "online", flag)) ping.online = flag;
    long ms = 0;
    if (doc.get_int_at("pings", i, "ms", ms)) {
      ping.ms = static_cast<uint16_t>(std::clamp<long>(ms, 0, 65535));
    }
  }

  // The preview keeps OTA hidden until the version label is clicked. The mock
  // version is stored for the simulated check result.
  doc.get_string("ota.version", ota_version_, sizeof(ota_version_));
  state_.ota.available = false;
  state_.ota.checked = false;
  state_.ota.downloading = false;
  state_.ota.progress = 0;
  std::snprintf(state_.ota.version, sizeof(state_.ota.version), "%s", ota_version_);
  read_string(doc, "ota.status", state_.ota.status, sizeof(state_.ota.status));

  state_.wifi_connected = get_bool(doc, "wifi.connected");
  state_.wifi_rssi =
      static_cast<int8_t>(clamp_value<long>(get_long(doc, "wifi.rssi", -60), -128, 127));
  read_string(doc, "wifi.ip", state_.ip, sizeof(state_.ip));

  read_string(doc, "fw_version", state_.fw_version, sizeof(state_.fw_version));
  read_string(doc, "note", state_.note, sizeof(state_.note));
  if (state_.weather.valid) state_.weather.weather_code = 3;  // Облачно

  if (state_.brightness == 0) state_.brightness = 80;
  return true;
}

void MockProvider::updateClock() {
#ifdef __EMSCRIPTEN__
  int hour = state_.time.hour;
  int minute = state_.time.minute;
  tablo_preview_local_time(&hour, &minute, state_.time.date_text,
                           static_cast<int>(sizeof(state_.time.date_text)));
  state_.time.hour = static_cast<uint8_t>(std::clamp<int>(hour, 0, 23));
  state_.time.minute = static_cast<uint8_t>(std::clamp<int>(minute, 0, 59));
  state_.time.valid = true;
#else
  if (state_.time.valid && (tick_count_ % 240u) == 0u) {
    if (++state_.time.minute >= 60) {
      state_.time.minute = 0;
      state_.time.hour = static_cast<uint8_t>((state_.time.hour + 1) % 24);
    }
  }
#endif
}

void MockProvider::updateDisplayMode() {
  if (state_.schedule.enabled) {
    const uint16_t minute_of_day =
        static_cast<uint16_t>(state_.time.hour) * 60U + state_.time.minute;
    state_.display_mode =
        display_mode_for(state_.schedule, minute_of_day, state_.time.valid);
  } else {
    state_.display_mode = state_.night_mode ? DisplayMode::Night
                                            : DisplayMode::Day;
  }
  state_.night_mode = state_.display_mode == DisplayMode::Night;
}

void MockProvider::processRequests() {
  if (update_check_pending_) {
    update_check_pending_ = false;
    state_.ota.checked = true;
    state_.ota.available = true;
    state_.ota.downloading = false;
    state_.ota.progress = 0;
    std::snprintf(state_.ota.version, sizeof(state_.ota.version), "%s", ota_version_);
    std::snprintf(state_.ota.status, sizeof(state_.ota.status), "Доступно обновление %s",
                  ota_version_);
  }

  if (install_pending_) {
    install_pending_ = false;
    if (state_.ota.available && !state_.ota.downloading) {
      state_.ota.downloading = true;
      state_.ota.progress = 0;
      std::snprintf(state_.ota.status, sizeof(state_.ota.status), "Загрузка обновления %s",
                    ota_version_);
    }
  }

  if (!state_.ota.downloading) return;

  const int step = 6 + static_cast<int>(nextRandom() % 11u);
  state_.ota.progress = std::min(100, state_.ota.progress + step);
  if (state_.ota.progress >= 100) {
    state_.ota.progress = 100;
    state_.ota.downloading = false;
    state_.ota.available = false;
    state_.ota.checked = true;
    std::snprintf(state_.fw_version, sizeof(state_.fw_version), "%s", ota_version_);
    std::snprintf(state_.ota.status, sizeof(state_.ota.status), "Установлено %s", ota_version_);
  } else {
    std::snprintf(state_.ota.status, sizeof(state_.ota.status), "Загрузка %d%%",
                  state_.ota.progress);
  }
}

uint32_t MockProvider::nextRandom() {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return rng_;
}

void MockProvider::applyFallback() {
  state_ = AppState{};
  state_.time.valid = true;
  state_.time.hour = 21;
  state_.time.minute = 47;
  std::snprintf(state_.time.date_text, sizeof(state_.time.date_text), "Превью");

  state_.weather.valid = true;
  state_.weather.temp_c = -3;
  state_.weather.feels_like_c = -6;
  state_.weather.pm25 = 12;
  state_.weather.aqi = 48;
  std::snprintf(state_.weather.condition, sizeof(state_.weather.condition), "Облачно");

  const PingInfo ping_defaults[kMaxPingTargets] = {
      {"GW", true, true, 1},
      {"VPS", true, true, 12},
      {"DNS", true, true, 8},
  };
  for (size_t i = 0; i < kMaxPingTargets; ++i) state_.pings[i] = ping_defaults[i];

  std::snprintf(ota_version_, sizeof(ota_version_), "1.0.6");
  state_.ota.available = false;
  state_.ota.checked = false;
  state_.ota.downloading = false;
  state_.ota.progress = 0;
  std::snprintf(state_.ota.version, sizeof(state_.ota.version), "%s", ota_version_);
  std::snprintf(state_.ota.status, sizeof(state_.ota.status), "Превью без mock_data.json");

  state_.wifi_connected = true;
  state_.wifi_rssi = -52;
  std::snprintf(state_.ip, sizeof(state_.ip), "192.0.2.42");
  std::snprintf(state_.fw_version, sizeof(state_.fw_version), "1.0.6");
  std::snprintf(state_.note, sizeof(state_.note), "mock_data.json не найден");
  state_.brightness = 80;

  state_.external.enabled = true;
  state_.external.configured = true;
  state_.external.valid = true;
  state_.external.online = true;
  state_.external.token_set = true;
  std::snprintf(state_.external.ip, sizeof(state_.external.ip),
                "192.0.2.50");
  std::snprintf(state_.external.model, sizeof(state_.external.model),
                "zhimi.airpurifier.mb3");
  std::snprintf(state_.external.status, sizeof(state_.external.status),
                "Подключено");
  state_.external.power = true;
  state_.external.power_valid = true;
  state_.external.mode = 0;
  state_.external.fan_level = 2;
  state_.external.pm25 = 8;
  state_.external.temperature = 23;
  state_.external.temperature_valid = true;
  state_.external.humidity = 42;
  state_.external.humidity_valid = true;
  state_.external.filter_life = 76;
  state_.external.updated_uptime_s = 3700;
}

MockProvider& mock_provider() {
  static MockProvider provider;
  return provider;
}

}  // namespace tablo
