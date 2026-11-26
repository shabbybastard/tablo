#pragma once

#include "net_worker.h"
#include "settings_nvs.h"

namespace tablo {
void platform_bind(NvsSettings* settings, NetWorker* net);

/** Consume all one-shot UI actions (bit mask of PlatformAction values). */
uint32_t platform_take_actions();
}
