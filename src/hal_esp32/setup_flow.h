#pragma once

#include "settings_nvs.h"
#include "wifi_manager.h"

namespace tablo {

/**
 * Blocking first-run wizard. Returns only through ESP.restart(); a failed
 * SoftAP start returns to the choice screen.
 */
void run_setup_flow(NvsSettings& settings, WifiManager& wifi);

}  // namespace tablo
