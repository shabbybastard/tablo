#include "miio_client.h"

#include <Arduino.h>
#include <mbedtls/aes.h>
#include <mbedtls/md5.h>

#include <cstdio>
#include <cstring>
#include <ctime>

namespace tablo {
namespace {

constexpr uint16_t kPort = 54321;
constexpr size_t kHeaderSize = 32;
constexpr uint32_t kHandshakeTimeoutMs = 900;
constexpr uint32_t kResponseTimeoutMs = 1200;

void set_error(char* error, size_t cap, const char* text) {
  if (error == nullptr || cap == 0) return;
  std::snprintf(error, cap, "%s", text != nullptr ? text : "");
}

uint32_t read_be32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) |
         static_cast<uint32_t>(data[3]);
}

void write_be32(uint8_t* data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value >> 24);
  data[1] = static_cast<uint8_t>(value >> 16);
  data[2] = static_cast<uint8_t>(value >> 8);
  data[3] = static_cast<uint8_t>(value);
}

void write_be16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value >> 8);
  data[1] = static_cast<uint8_t>(value);
}

void md5_digest(const uint8_t* data, size_t len, uint8_t out[16]) {
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, data, len);
  mbedtls_md5_finish(&ctx, out);
  mbedtls_md5_free(&ctx);
}

void md5_parts(const uint8_t* first, size_t first_len, const uint8_t* second,
               size_t second_len, const uint8_t* third, size_t third_len,
               uint8_t out[16]) {
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, first, first_len);
  if (second != nullptr && second_len > 0) {
    mbedtls_md5_update(&ctx, second, second_len);
  }
  if (third != nullptr && third_len > 0) {
    mbedtls_md5_update(&ctx, third, third_len);
  }
  mbedtls_md5_finish(&ctx, out);
  mbedtls_md5_free(&ctx);
}

void key_iv(const uint8_t token[kMiioTokenBytes], uint8_t key[16],
            uint8_t iv[16]) {
  md5_digest(token, kMiioTokenBytes, key);
  uint8_t input[32];
  std::memcpy(input, key, 16);
  std::memcpy(input + 16, token, kMiioTokenBytes);
  md5_digest(input, sizeof(input), iv);
}

bool aes_cbc(bool encrypt, const uint8_t key[16], const uint8_t iv_in[16],
             const uint8_t* in, size_t len, uint8_t* out) {
  if (len == 0 || (len % 16) != 0) return false;
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  const int key_result =
      encrypt ? mbedtls_aes_setkey_enc(&ctx, key, 128)
              : mbedtls_aes_setkey_dec(&ctx, key, 128);
  int result = key_result;
  if (result == 0) {
    uint8_t iv[16];
    std::memcpy(iv, iv_in, sizeof(iv));
    result = mbedtls_aes_crypt_cbc(
        &ctx, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, len, iv, in,
        out);
  }
  mbedtls_aes_free(&ctx);
  return result == 0;
}

int receive_packet(WiFiUDP& udp, uint8_t* buffer, size_t cap,
                   uint32_t timeout_ms) {
  const uint32_t started = millis();
  while (millis() - started < timeout_ms) {
    const int available = udp.parsePacket();
    if (available > 0) {
      if (static_cast<size_t>(available) > cap) {
        return -2;
      }
      const int read = udp.read(buffer, cap);
      if (read > 0) return read;
    }
    delay(5);
  }
  return -1;
}

bool send_packet(WiFiUDP& udp, const IPAddress& ip, const uint8_t* data,
                 size_t len) {
  if (!udp.beginPacket(ip, kPort)) return false;
  if (udp.write(data, len) != len) {
    udp.endPacket();
    return false;
  }
  return udp.endPacket() == 1;
}

}  // namespace

void MiioClient::reset() {
  device_id_ = 0;
  device_ts_ = 0;
  next_id_ = 1;
}

bool MiioClient::poll(const char* ip_text,
                      const uint8_t token[kMiioTokenBytes],
                      const char* model_hint, MiioReading& reading,
                      char* model_out, size_t model_cap, char* error,
                      size_t error_cap) {
  reading = MiioReading{};
  if (model_out != nullptr && model_cap > 0) model_out[0] = '\0';
  set_error(error, error_cap, "");

  if (ip_text == nullptr || ip_text[0] == '\0' || token == nullptr) {
    set_error(error, error_cap, "Не заданы IP и токен");
    return false;
  }

  IPAddress ip;
  if (!ip.fromString(ip_text)) {
    set_error(error, error_cap, "Неверный IP");
    return false;
  }

  WiFiUDP udp;
  if (!udp.begin(0)) {
    set_error(error, error_cap, "Не удалось открыть UDP");
    return false;
  }
  const bool ok = poll_with_socket(udp, ip, token, model_hint, reading,
                                   model_out, model_cap, error, error_cap);
  udp.stop();
  return ok;
}

bool MiioClient::handshake(WiFiUDP& udp, const IPAddress& ip,
                           const uint8_t token[kMiioTokenBytes], char* error,
                           size_t error_cap) {
  (void)token;  // The hello response carries the token for discovery; we
                // already have it from settings.

  uint8_t hello[kHeaderSize];
  std::memset(hello, 0xFF, sizeof(hello));
  hello[0] = 0x21;
  hello[1] = 0x31;
  hello[2] = 0x00;
  hello[3] = 0x20;

  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!send_packet(udp, ip, hello, sizeof(hello))) {
      continue;
    }
    const int length = receive_packet(udp, packet_, sizeof(packet_),
                                      kHandshakeTimeoutMs);
    if (length >= static_cast<int>(kHeaderSize) && packet_[0] == 0x21 &&
        packet_[1] == 0x31 &&
        ((static_cast<uint16_t>(packet_[2]) << 8) | packet_[3]) ==
            kHeaderSize) {
      device_id_ = read_be32(packet_ + 8);
      const uint32_t stamp = read_be32(packet_ + 12);
      device_ts_ = stamp != 0 ? stamp : static_cast<uint32_t>(time(nullptr));
      return true;
    }
  }

  set_error(error, error_cap, "Устройство не отвечает");
  return false;
}

bool MiioClient::transact(WiFiUDP& udp, const IPAddress& ip,
                          const uint8_t token[kMiioTokenBytes],
                          const char* request_json, char* error,
                          size_t error_cap) {
  if (request_json == nullptr) {
    set_error(error, error_cap, "Пустой запрос");
    return false;
  }
  const size_t json_len = std::strlen(request_json);
  const size_t plain_len = json_len + 1;  // trailing NUL, as in python-miio
  const size_t padded_len = (plain_len / 16 + 1) * 16;
  if (padded_len > sizeof(packet_) - kHeaderSize) {
    set_error(error, error_cap, "Запрос слишком длинный");
    return false;
  }

  uint8_t key[16];
  uint8_t iv[16];
  key_iv(token, key, iv);

  std::memcpy(plain_, request_json, json_len);
  plain_[json_len] = 0;
  const uint8_t pad = static_cast<uint8_t>(padded_len - plain_len);
  std::memset(plain_ + plain_len, pad, pad);

  if (!aes_cbc(true, key, iv, plain_, padded_len,
               packet_ + kHeaderSize)) {
    set_error(error, error_cap, "Ошибка шифрования");
    return false;
  }

  packet_[0] = 0x21;
  packet_[1] = 0x31;
  write_be16(packet_ + 2, static_cast<uint16_t>(kHeaderSize + padded_len));
  write_be32(packet_ + 4, 0);
  write_be32(packet_ + 8, device_id_);
  device_ts_ += 1;
  write_be32(packet_ + 12, device_ts_);

  uint8_t checksum[16];
  md5_parts(packet_, 16, token, kMiioTokenBytes, packet_ + kHeaderSize,
            padded_len, checksum);
  std::memcpy(packet_ + 16, checksum, sizeof(checksum));

  const size_t packet_len = kHeaderSize + padded_len;
  if (!send_packet(udp, ip, packet_, packet_len)) {
    set_error(error, error_cap, "Не удалось отправить запрос");
    return false;
  }

  const int length =
      receive_packet(udp, packet_, sizeof(packet_), kResponseTimeoutMs);
  if (length < static_cast<int>(kHeaderSize)) {
    set_error(error, error_cap, "Устройство не отвечает");
    return false;
  }
  if (packet_[0] != 0x21 || packet_[1] != 0x31) {
    set_error(error, error_cap, "Ошибка ответа");
    return false;
  }
  const uint16_t declared =
      (static_cast<uint16_t>(packet_[2]) << 8) | packet_[3];
  if (declared != static_cast<uint16_t>(length)) {
    set_error(error, error_cap, "Ошибка ответа");
    return false;
  }

  const size_t payload_len = static_cast<size_t>(length) - kHeaderSize;
  if (payload_len == 0 || (payload_len % 16) != 0 ||
      payload_len > sizeof(plain_)) {
    set_error(error, error_cap, "Ошибка ответа");
    return false;
  }

  uint8_t expected[16];
  md5_parts(packet_, 16, token, kMiioTokenBytes, packet_ + kHeaderSize,
            payload_len, expected);
  if (std::memcmp(expected, packet_ + 16, sizeof(expected)) != 0) {
    set_error(error, error_cap, "Неверный токен");
    return false;
  }

  if (!aes_cbc(false, key, iv, packet_ + kHeaderSize, payload_len, plain_)) {
    set_error(error, error_cap, "Ошибка расшифровки");
    return false;
  }

  const uint8_t padding = plain_[payload_len - 1];
  if (padding < 1 || padding > 16 || padding > payload_len) {
    set_error(error, error_cap, "Ошибка токена");
    return false;
  }
  for (size_t i = 0; i < padding; ++i) {
    if (plain_[payload_len - 1 - i] != padding) {
      set_error(error, error_cap, "Ошибка токена");
      return false;
    }
  }
  size_t decoded_len = payload_len - padding;

  /* The payload is JSON followed by a NUL byte and PKCS#7 padding. */
  size_t json_end = 0;
  while (json_end < decoded_len && plain_[json_end] != 0) ++json_end;
  if (json_end == 0 || json_end >= sizeof(response_)) {
    set_error(error, error_cap, "Ошибка ответа");
    return false;
  }
  std::memcpy(response_, plain_, json_end);
  response_[json_end] = '\0';

  const uint32_t stamp = read_be32(packet_ + 12);
  if (stamp != 0) device_ts_ = stamp;

  return true;
}

bool MiioClient::request_info(WiFiUDP& udp, const IPAddress& ip,
                              const uint8_t token[kMiioTokenBytes],
                              char* model, size_t model_cap, char* error,
                              size_t error_cap) {
  const uint16_t id = next_id_++;
  if (next_id_ >= 9999) next_id_ = 1;
  char request[96];
  const int written = std::snprintf(
      request, sizeof(request),
      "{\"id\":%u,\"method\":\"miIO.info\",\"params\":[]}",
      static_cast<unsigned>(id));
  if (written < 0 || static_cast<size_t>(written) >= sizeof(request)) {
    set_error(error, error_cap, "Ошибка запроса");
    return false;
  }
  if (!transact(udp, ip, token, request, error, error_cap)) return false;
  return miio_parse_model(response_, std::strlen(response_), model, model_cap);
}

bool MiioClient::poll_with_socket(
    WiFiUDP& udp, const IPAddress& ip, const uint8_t token[kMiioTokenBytes],
    const char* model_hint, MiioReading& reading, char* model_out,
    size_t model_cap, char* error, size_t error_cap) {
  if (!handshake(udp, ip, token, error, error_cap)) return false;

  char model[32] = {};
  if (model_hint != nullptr && model_hint[0] != '\0') {
    std::snprintf(model, sizeof(model), "%s", model_hint);
  } else if (!request_info(udp, ip, token, model, sizeof(model), error,
                           error_cap)) {
    /* Some firmware variants do not answer miIO.info; the property request
     * below can still work, so keep the fallback mapping. */
    model[0] = '\0';
  }

  if (model_out != nullptr && model_cap > 0) {
    std::snprintf(model_out, model_cap, "%s", model);
  }
  if (model[0] != '\0' && !miio_model_supported(model)) {
    set_error(error, error_cap, "Модель не поддерживается");
    return false;
  }

  const MiioMapping* mapping = miio_mapping_for_model(model);
  bool has_data = false;
  char miot_error[48] = {};

  const uint16_t miot_id = next_id_++;
  if (next_id_ >= 9999) next_id_ = 1;
  if (!mapping->legacy_get_prop) {
    char request[1024];
    const size_t request_len =
        miio_build_get_properties(miot_id, *mapping, request, sizeof(request));
    if (request_len == 0) {
      set_error(error, error_cap, "Ошибка запроса");
      return false;
    }
    if (transact(udp, ip, token, request, error, error_cap)) {
      has_data =
          miio_parse_miot_properties(response_, std::strlen(response_), reading);
      if (!has_data) {
        std::snprintf(miot_error, sizeof(miot_error), "Ошибка ответа");
      }
    } else {
      if (error != nullptr) {
        std::snprintf(miot_error, sizeof(miot_error), "%s", error);
      }
    }
  }

  if (!has_data) {
    const uint16_t legacy_id = next_id_++;
    if (next_id_ >= 9999) next_id_ = 1;
    char request[256];
    const size_t request_len =
        miio_build_get_prop(legacy_id, request, sizeof(request));
    if (request_len == 0) {
      set_error(error, error_cap, "Ошибка запроса");
      return false;
    }
    if (transact(udp, ip, token, request, error, error_cap)) {
      has_data =
          miio_parse_legacy_props(response_, std::strlen(response_), reading);
      if (!has_data) {
        std::snprintf(miot_error, sizeof(miot_error), "Ошибка ответа");
      }
    }
  }

  if (!has_data) {
    if (miot_error[0] != '\0') {
      set_error(error, error_cap, miot_error);
    } else if (error != nullptr && error[0] == '\0') {
      set_error(error, error_cap, "Нет данных");
    }
    return false;
  }

  const bool meaningful =
      reading.pm25 >= 0 || reading.filter_life >= 0 ||
      reading.has_temperature || reading.has_humidity;
  if (!meaningful) {
    set_error(error, error_cap, "Нет данных");
    return false;
  }

  if (model_out != nullptr && model_cap > 0) {
    std::snprintf(model_out, model_cap, "%s",
                  model[0] != '\0' ? model : mapping->family);
  }
  if (error != nullptr && error_cap > 0) error[0] = '\0';
  return true;
}

}  // namespace tablo
