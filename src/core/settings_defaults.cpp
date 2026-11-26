#include "settings.h"

#include <cstddef>
#include <cstring>

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif

namespace tablo {
namespace {

/** Bounded string copy that always NUL-terminates (and tolerates cap == 0). */
[[maybe_unused]] void copy_text(char* dst, size_t cap, const char* src) {
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

}  // namespace

Settings default_settings() {
  Settings settings;

  /* Values from secrets.h are optional: each macro is applied only when the
   * local (gitignored) file defines it. Defaults above stay in place
   * otherwise. Never commit real credentials. */
#if defined(TABLO_WIFI_SSID)
  copy_text(settings.wifi_ssid, sizeof(settings.wifi_ssid), TABLO_WIFI_SSID);
#endif
#if defined(TABLO_WIFI_PASSWORD)
  copy_text(settings.wifi_password, sizeof(settings.wifi_password), TABLO_WIFI_PASSWORD);
#endif

#if defined(TABLO_ADMIN_USER)
  copy_text(settings.admin_user, sizeof(settings.admin_user), TABLO_ADMIN_USER);
#endif
#if defined(TABLO_ADMIN_PASSWORD)
  copy_text(settings.admin_password, sizeof(settings.admin_password), TABLO_ADMIN_PASSWORD);
#endif

#if defined(TABLO_OTA_MANIFEST_URL)
  copy_text(settings.ota_manifest_url, sizeof(settings.ota_manifest_url),
            TABLO_OTA_MANIFEST_URL);
#endif

#if defined(TABLO_WEATHER_LAT)
  copy_text(settings.weather_lat, sizeof(settings.weather_lat), TABLO_WEATHER_LAT);
#endif
#if defined(TABLO_WEATHER_LON)
  copy_text(settings.weather_lon, sizeof(settings.weather_lon), TABLO_WEATHER_LON);
#endif

#if defined(TABLO_PING_GW)
  copy_text(settings.ping_gw, sizeof(settings.ping_gw), TABLO_PING_GW);
#endif
#if defined(TABLO_PING_VPS)
  copy_text(settings.ping_vps, sizeof(settings.ping_vps), TABLO_PING_VPS);
#endif
#if defined(TABLO_PING_DNS)
  copy_text(settings.ping_dns, sizeof(settings.ping_dns), TABLO_PING_DNS);
#endif

  return settings;
}

}  // namespace tablo
