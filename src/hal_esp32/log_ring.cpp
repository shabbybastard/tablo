#include "log_ring.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace tablo {
namespace {
constexpr size_t kLines = 100;
constexpr size_t kLineLen = 160;
char g_lines[kLines][kLineLen];
size_t g_head = 0;
size_t g_count = 0;
std::mutex g_log_mutex;

void copy_line(char* dst, const char* src) {
  std::strncpy(dst, src, kLineLen - 1);
  dst[kLineLen - 1] = '\0';
}
}  // namespace

void log_init() {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  g_head = 0;
  g_count = 0;
  for (auto& line : g_lines) line[0] = '\0';
}

void log_line(const char* level, const char* fmt, ...) {
  char body[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(body, sizeof(body), fmt, args);
  va_end(args);

  char line[kLineLen];
  snprintf(line, sizeof(line), "[%8lu] %s %s", static_cast<unsigned long>(millis()),
           level ? level : "?", body);

  {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    copy_line(g_lines[g_head], line);
    g_head = (g_head + 1) % kLines;
    if (g_count < kLines) g_count++;
  }
  Serial.println(line);
}

size_t log_count() {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  return g_count;
}

void log_snapshot(char* out, size_t cap) {
  if (!out || cap == 0) return;
  out[0] = '\0';
  std::lock_guard<std::mutex> lock(g_log_mutex);
  size_t written = 0;
  const size_t start = (g_count == kLines) ? g_head : 0;
  for (size_t i = 0; i < g_count; ++i) {
    const char* line = g_lines[(start + i) % kLines];
    const size_t len = std::strlen(line);
    if (written + len + 2 >= cap) break;
    std::memcpy(out + written, line, len);
    written += len;
    out[written++] = '\n';
    out[written] = '\0';
  }
}

}  // namespace tablo
