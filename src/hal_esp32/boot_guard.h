#pragma once

#include <cstdint>

namespace tablo {

void boot_guard_begin();
bool boot_guard_in_rescue();
void boot_guard_mark_stable();
void boot_guard_clear();
uint32_t boot_guard_failures();

}  // namespace tablo
