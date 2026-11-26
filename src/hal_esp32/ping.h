#pragma once

#include <cstdint>

namespace tablo {

/** One blocking ICMP echo request; returns false on DNS/timeout/error. */
bool ping_host(const char* host, uint16_t& rtt_ms, uint32_t timeout_ms = 1200);

}  // namespace tablo
