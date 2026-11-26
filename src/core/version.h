#pragma once

#ifndef FW_VERSION
// Builds always inject FW_VERSION from VERSION (scripts/pio_build_info.py).
// This fallback only marks builds that bypass the build script.
#define FW_VERSION "0.0.0-dev"
#endif

namespace tablo {
constexpr const char* kFwVersion = FW_VERSION;
}
