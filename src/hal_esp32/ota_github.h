#pragma once

#include "core/app_state.h"
#include "core/parsers.h"
#include "core/settings.h"

namespace tablo {

/** Fetch and compare the GitHub Pages manifest. Returns true if the check ran. */
bool ota_check_manifest(const Settings& settings, ManifestInfo& manifest, OtaInfo& ota);

/** Download + verify + install. Reboots on success; restores UI on failure. */
bool ota_install(const Settings& settings, const ManifestInfo& manifest, OtaInfo& ota);

}  // namespace tablo
