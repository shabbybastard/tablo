#pragma once

#include "core/settings.h"

namespace tablo {

/** True when the device still needs the first-run setup screen. */
bool settings_need_setup(const Settings& settings);

}  // namespace tablo
