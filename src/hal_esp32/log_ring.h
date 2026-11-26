#pragma once

#include <cstddef>
#include <cstdint>

namespace tablo {

void log_init();
void log_line(const char* level, const char* fmt, ...);
void log_snapshot(char* out, size_t cap);
size_t log_count();

}  // namespace tablo

#define LOGI(...) ::tablo::log_line("I", __VA_ARGS__)
#define LOGW(...) ::tablo::log_line("W", __VA_ARGS__)
#define LOGE(...) ::tablo::log_line("E", __VA_ARGS__)
