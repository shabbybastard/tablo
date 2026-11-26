#include "boot_guard.h"

#include <esp_attr.h>

#include "log_ring.h"

namespace tablo {
namespace {
constexpr uint32_t kMagic = 0x5441424CU;  // "TABL"
constexpr uint32_t kRescueThreshold = 3;
RTC_NOINIT_ATTR uint32_t g_magic;
RTC_NOINIT_ATTR uint32_t g_failures;
}  // namespace

void boot_guard_begin() {
  if (g_magic != kMagic) {
    g_magic = kMagic;
    g_failures = 0;
  }
  if (g_failures < 1000) {
    g_failures++;
  }
  LOGI("boot: attempt %lu (rescue after %lu failed boots)",
       static_cast<unsigned long>(g_failures), static_cast<unsigned long>(kRescueThreshold));
}

bool boot_guard_in_rescue() {
  return g_failures > kRescueThreshold;
}

uint32_t boot_guard_failures() {
  return g_failures;
}

void boot_guard_mark_stable() {
  if (g_failures != 0) {
    g_failures = 0;
    LOGI("boot: stable for 10 s, rescue counter cleared");
  }
}

void boot_guard_clear() {
  g_failures = 0;
  g_magic = kMagic;
}

}  // namespace tablo
