#pragma once

#include <cstddef>
#include <cstdint>

#include "core/date_format.h"
#include "core/schedule.h"

namespace tablo {

constexpr size_t kMaxPingTargets = 3;
constexpr size_t kMaxWifiScan = 12;

struct TimeInfo {
  bool valid = false;
  uint8_t hour = 0;
  uint8_t minute = 0;
  char date_text[32] = "";
};

struct WeatherInfo {
  bool valid = false;
  int16_t temp_c = 0;
  int16_t feels_like_c = 0;
  int16_t pm25 = 0;
  int16_t aqi = 0;
  int16_t weather_code = -1;
  char condition[24] = "";
  char status[40] = "";
};

struct PingInfo {
  char label[8] = "";
  bool configured = false;
  bool online = false;
  uint16_t ms = 0;
};

/** Local air purifier data, polled over the miIO protocol. */
struct ExternalInfo {
  bool enabled = false;
  bool configured = false;
  bool valid = false;   // at least one successful poll
  bool online = false;  // the latest poll succeeded
  char ip[16] = "";
  bool token_set = false;
  char model[32] = "";
  char status[48] = "";
  bool power = false;
  bool power_valid = false;
  int16_t mode = -1;
  int16_t fan_level = -1;
  int16_t pm25 = -1;
  int16_t temperature = 0;
  bool temperature_valid = false;
  int16_t humidity = 0;
  bool humidity_valid = false;
  int16_t filter_life = -1;
  int16_t tvoc = -1;
  uint32_t updated_uptime_s = 0;
};

struct OtaInfo {
  bool available = false;
  bool downloading = false;
  bool checked = false;
  bool error = false;
  int progress = 0;
  char version[24] = "";
  char status[48] = "";
};

enum class SetupMode : uint8_t {
  None = 0,
  Choice,
  Wifi,
  Sd,
};

struct SetupInfo {
  bool active = false;
  SetupMode mode = SetupMode::None;
  bool card_present = false;
  bool card_mounted = false;
  char status[192] = "";
  char hint[192] = "";
  char ap_ssid[24] = "";
  char ap_password[40] = "";
};

struct SdImportInfo {
  bool pending = false;
  bool invalid = false;
  bool applying = false;
  uint32_t event_id = 0;
  char file[48] = "";
  char error[192] = "";
};

struct WifiScanEntry {
  char ssid[33] = "";
  int8_t rssi = 0;
  bool secure = false;
};

struct WifiScanInfo {
  bool scanning = false;
  uint8_t count = 0;
  uint32_t generation = 0;
  WifiScanEntry entries[kMaxWifiScan];
};

struct AppState {
  TimeInfo time;
  WeatherInfo weather;
  ExternalInfo external;
  PingInfo pings[kMaxPingTargets];
  OtaInfo ota;
  bool wifi_connected = false;
  int8_t wifi_rssi = 0;
  char ip[16] = "";
  uint8_t brightness = 80;
  bool night_mode = false;
  DisplayMode display_mode = DisplayMode::Day;
  NightSchedule schedule;
  DateFormat date_format = DateFormat::DayMonthWeekday;
  bool weather_enabled = true;
  char timezone[48] = "MSK-3";
  WifiScanInfo wifi_scan;
  char wifi_ssid[33] = "";
  char wifi_status[64] = "";
  char fw_version[24] = "";
  char note[96] = "";
  uint32_t uptime_s = 0;
  uint16_t cpu_mhz = 0;
  uint32_t flash_used = 0;
  uint32_t flash_total = 0;
  uint32_t ram_free = 0;
  uint32_t ram_total = 0;
  uint32_t revision = 0;
  SetupInfo setup;
  SdImportInfo sd_import;
};

constexpr size_t kPingGateway = 0;
constexpr size_t kPingVps = 1;
constexpr size_t kPingDns = 2;

}  // namespace tablo
