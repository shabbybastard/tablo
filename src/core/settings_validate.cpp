#include "settings_validate.h"

#include <cstring>

namespace tablo {

bool settings_need_setup(const Settings& settings) {
  return settings.wifi_ssid[0] == '\0' || settings.admin_user[0] == '\0' ||
         settings.admin_password[0] == '\0' ||
         std::strcmp(settings.admin_password, "change-me") == 0;
}

}  // namespace tablo
