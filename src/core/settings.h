#pragma once

#include <cstdint>

#include "core/date_format.h"
#include "core/schedule.h"

namespace tablo {

struct Settings {
  char wifi_ssid[33] = "";
  char wifi_password[65] = "";

  char admin_user[32] = "admin";
  char admin_password[64] = "change-me";

  char timezone[48] = "MSK-3";
  char weather_lat[16] = "";
  char weather_lon[16] = "";

  char ping_gw[64] = "";
  char ping_vps[64] = "";
  char ping_dns[64] = "";

  char ota_manifest_url[192] = "https://shabbybastard.github.io/tablo/manifest.json";
  char note[96] = "Все системы в норме";

  uint8_t brightness = 80;
  bool night_mode = false;
  bool auto_ota = true;
  NightSchedule schedule;
  DateFormat date_format = DateFormat::DayMonthWeekday;
  bool weather_enabled = true;

  /* Xiaomi air purifier (miIO). Appended after the version-2 settings layout;
   * keep new fields at the end so the NVS migration can read old blobs. */
  bool purifier_enabled = false;
  char purifier_ip[16] = "";
  char purifier_token[33] = "";
};

/** Defaults built from include/secrets.h when present, otherwise placeholders. */
Settings default_settings();

class ISettingsStore {
 public:
  virtual ~ISettingsStore() = default;
  virtual Settings get() const = 0;
  virtual void save(const Settings& settings) = 0;
};

}  // namespace tablo
