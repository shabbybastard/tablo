#include "ping.h"

#include "ping/ping_sock.h"
extern "C" {
#include <lwip/netdb.h>
#include <lwip/sockets.h>
}
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstring>

#include "log_ring.h"

namespace tablo {
namespace {
struct PingState {
  SemaphoreHandle_t done;
  bool success;
  uint32_t time_ms;
};

void on_ping_success(esp_ping_handle_t handle, void* args) {
  auto* state = static_cast<PingState*>(args);
  uint32_t time_ms = 0;
  esp_ping_get_profile(handle, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms));
  state->success = true;
  state->time_ms = time_ms;
}

void on_ping_timeout(esp_ping_handle_t, void*) {}

void on_ping_end(esp_ping_handle_t, void* args) {
  auto* state = static_cast<PingState*>(args);
  xSemaphoreGive(state->done);
}

bool resolve_ipv4(const char* host, ip4_addr_t& out) {
  addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* result = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &result) != 0 || result == nullptr) {
    return false;
  }
  const auto* addr = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
  out.addr = addr->sin_addr.s_addr;
  freeaddrinfo(result);
  return true;
}
}  // namespace

bool ping_host(const char* host, uint16_t& rtt_ms, uint32_t timeout_ms) {
  rtt_ms = 0;
  if (!host || host[0] == '\0') return false;

  ip4_addr_t ip = {};
  if (!resolve_ipv4(host, ip)) {
    LOGW("ping: cannot resolve %s", host);
    return false;
  }

  PingState state = {};
  state.done = xSemaphoreCreateBinary();
  if (!state.done) return false;

  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  config.target_addr.type = IPADDR_TYPE_V4;
  config.target_addr.u_addr.ip4.addr = ip.addr;
  config.count = 1;
  config.interval_ms = 10;
  config.timeout_ms = timeout_ms;
  config.data_size = 32;

  esp_ping_callbacks_t callbacks = {};
  callbacks.on_ping_success = on_ping_success;
  callbacks.on_ping_timeout = on_ping_timeout;
  callbacks.on_ping_end = on_ping_end;
  callbacks.cb_args = &state;

  esp_ping_handle_t handle = nullptr;
  bool ok = false;
  if (esp_ping_new_session(&config, &callbacks, &handle) == ESP_OK && handle) {
    if (esp_ping_start(handle) == ESP_OK) {
      if (xSemaphoreTake(state.done, pdMS_TO_TICKS(timeout_ms + 1500)) == pdTRUE) {
        ok = state.success;
        if (ok) {
          rtt_ms = static_cast<uint16_t>(state.time_ms > 65535 ? 65535 : state.time_ms);
        }
      }
    }
    esp_ping_delete_session(handle);
  }
  vSemaphoreDelete(state.done);
  return ok;
}

}  // namespace tablo
