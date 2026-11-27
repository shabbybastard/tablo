#pragma once

#include <cstddef>
#include <cstdint>

#include <WiFiUdp.h>

#include "core/miio.h"

namespace tablo {

/**
 * Minimal synchronous miIO client for Xiaomi air purifiers.
 *
 * The miIO protocol is UDP/54321 with an AES-128-CBC payload keyed by the
 * device token. A short handshake returns the device id and timestamp; every
 * following command is encrypted and checksummed. Only the net task may use
 * this class.
 */
class MiioClient {
 public:
  MiioClient() = default;
  MiioClient(const MiioClient&) = delete;
  MiioClient& operator=(const MiioClient&) = delete;

  /** Forget the session after a Wi-Fi reconnect. */
  void reset();

  /**
   * Poll one device. `model_hint` may contain a previously detected model to
   * skip the `miIO.info` request. The poll is bounded by short UDP timeouts
   * and never logs the token.
   */
  bool poll(const char* ip_text, const uint8_t token[kMiioTokenBytes],
            const char* model_hint, MiioReading& reading, char* model_out,
            size_t model_cap, char* error, size_t error_cap);

 private:
  bool poll_with_socket(WiFiUDP& udp, const IPAddress& ip,
                        const uint8_t token[kMiioTokenBytes],
                        const char* model_hint, MiioReading& reading,
                        char* model_out, size_t model_cap, char* error,
                        size_t error_cap);
  bool handshake(WiFiUDP& udp, const IPAddress& ip,
                 const uint8_t token[kMiioTokenBytes], char* error,
                 size_t error_cap);
  bool request_info(WiFiUDP& udp, const IPAddress& ip,
                    const uint8_t token[kMiioTokenBytes], char* model,
                    size_t model_cap, char* error, size_t error_cap);
  bool transact(WiFiUDP& udp, const IPAddress& ip,
                const uint8_t token[kMiioTokenBytes],
                const char* request_json, char* error, size_t error_cap);

  uint32_t device_id_ = 0;
  uint32_t device_ts_ = 0;
  uint16_t next_id_ = 1;

  char response_[1536] = {};
  uint8_t packet_[1536] = {};
  uint8_t plain_[1536] = {};
};

}  // namespace tablo
