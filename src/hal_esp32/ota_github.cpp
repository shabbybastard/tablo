#include "ota_github.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

#include <cstring>

#include "backlight.h"
#include "boot_guard.h"
#include "core/state_store.h"
#include "core/version.h"
#include "http_client.h"
#include "log_ring.h"
#include "render.h"

namespace tablo {
namespace {
void publish_ota(const OtaInfo& ota) { g_state.update_ota(ota); }

void sha_to_hex(const unsigned char digest[32], char out[65]) {
  static const char* hex = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) {
    out[i * 2] = hex[(digest[i] >> 4) & 0xF];
    out[i * 2 + 1] = hex[digest[i] & 0xF];
  }
  out[64] = '\0';
}

bool equal_hex(const char* a, const char* b) {
  if (!a || !b || std::strlen(a) != 64 || std::strlen(b) != 64) return false;
  for (int i = 0; i < 64; ++i) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'F') ca = static_cast<char>(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'F') cb = static_cast<char>(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return true;
}
}  // namespace

bool ota_check_manifest(const Settings& settings, ManifestInfo& manifest, OtaInfo& ota) {
  ota.checked = true;
  if (settings.ota_manifest_url[0] == '\0') {
    snprintf(ota.status, sizeof(ota.status), "URL манифеста не задан");
    return false;
  }
  if (!String(settings.ota_manifest_url).startsWith("https://")) {
    snprintf(ota.status, sizeof(ota.status), "Manifest URL должен быть https");
    ota.available = false;
    LOGW("ota: manifest URL must be https");
    return false;
  }
  HttpResponse response;
  if (!http_get(settings.ota_manifest_url, 8192, response) ||
      response.status != 200) {
    snprintf(ota.status, sizeof(ota.status), "Манифест недоступен (%d)", response.status);
    ota.available = false;
    LOGW("ota: manifest check failed, status=%d", response.status);
    return false;
  }
  ManifestInfo parsed;
  if (!parse_manifest(response.body.c_str(), response.body.length(), parsed)) {
    snprintf(ota.status, sizeof(ota.status), "Некорректный manifest.json");
    ota.available = false;
    LOGW("ota: invalid manifest");
    return false;
  }
  manifest = parsed;
  if (version_is_newer(parsed.version, kFwVersion)) {
    ota.available = true;
    snprintf(ota.version, sizeof(ota.version), "%s", parsed.version);
    snprintf(ota.status, sizeof(ota.status), "Доступна %s", parsed.version);
    LOGI("ota: update available %s -> %s", kFwVersion, parsed.version);
  } else {
    ota.available = false;
    snprintf(ota.version, sizeof(ota.version), "%s", parsed.version);
    snprintf(ota.status, sizeof(ota.status), "Последняя версия");
    LOGI("ota: already up to date (%s)", kFwVersion);
  }
  return true;
}

bool ota_install(const Settings& settings, const ManifestInfo& manifest, OtaInfo& ota) {
  if (!manifest.bin_url[0] || !manifest.sha256[0]) {
    snprintf(ota.status, sizeof(ota.status), "В манифесте нет bin/sha256");
    return false;
  }
  if (!String(manifest.bin_url).startsWith("https://")) {
    snprintf(ota.status, sizeof(ota.status), "bin_url должен быть https");
    return false;
  }

  LOGI("ota: downloading %s", manifest.bin_url);
  ota.downloading = true;
  ota.progress = 0;
  snprintf(ota.status, sizeof(ota.status), "Загрузка %s", manifest.version);
  publish_ota(ota);

  if (!render_pause()) {
    snprintf(ota.status, sizeof(ota.status), "Рендер не остановился");
    ota.downloading = false;
    publish_ota(ota);
    apply_current_backlight();
    return false;
  }
  backlight_set(0);

  WiFiClientSecure client;
  client.setHandshakeTimeout(8);
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setReuse(false);
  http.setUserAgent(String("tablo/") + kFwVersion);
  if (!http.begin(client, manifest.bin_url)) {
    snprintf(ota.status, sizeof(ota.status), "HTTP begin failed");
    render_resume();
    apply_current_backlight();
    ota.downloading = false;
    return false;
  }
  const int status = http.GET();
  if (status != 200) {
    snprintf(ota.status, sizeof(ota.status), "HTTP %d", status);
    http.end();
    client.stop();
    render_resume();
    apply_current_backlight();
    ota.downloading = false;
    return false;
  }

  const int total = http.getSize();
  if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN)) {
    snprintf(ota.status, sizeof(ota.status), "Нет места в OTA-слоте");
    Update.printError(Serial);
    http.end();
    client.stop();
    render_resume();
    apply_current_backlight();
    ota.downloading = false;
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  uint8_t buf[2048];
  size_t written = 0;
  bool ok = true;
  uint32_t last_data = millis();
  Stream* stream = http.getStreamPtr();
  while (stream && (http.connected() || stream->available())) {
    const int n = stream->readBytes(buf, sizeof(buf));
    if (n <= 0) {
      if (!http.connected()) break;
      if (millis() - last_data > 20000) {
        ok = false;
        snprintf(ota.status, sizeof(ota.status), "Таймаут загрузки");
        break;
      }
      delay(5);
      continue;
    }
    last_data = millis();
    if (Update.write(buf, n) != static_cast<size_t>(n)) {
      ok = false;
      snprintf(ota.status, sizeof(ota.status), "Ошибка записи flash");
      break;
    }
    mbedtls_sha256_update(&sha, buf, n);
    written += n;
    if (total > 0) {
      ota.progress = static_cast<int>((written * 100ULL) / static_cast<uint64_t>(total));
      if ((ota.progress % 10) == 0) publish_ota(ota);
    }
  }

  unsigned char digest[32] = {};
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  char actual[65];
  sha_to_hex(digest, actual);
  http.end();
  client.stop();

  if (!ok || !equal_hex(actual, manifest.sha256)) {
    if (ok) {
      snprintf(ota.status, sizeof(ota.status), "SHA-256 не совпал");
      LOGE("ota: sha mismatch expected=%s actual=%s", manifest.sha256, actual);
    }
    Update.abort();
    render_resume();
    apply_current_backlight();
    ota.downloading = false;
    ota.progress = 0;
    return false;
  }

  if (!Update.end(true)) {
    snprintf(ota.status, sizeof(ota.status), "Update.end failed");
    Update.printError(Serial);
    Update.abort();
    render_resume();
    apply_current_backlight();
    ota.downloading = false;
    return false;
  }

  LOGI("ota: installed %s (%u bytes), rebooting", manifest.version,
       static_cast<unsigned>(written));
  ota.downloading = false;
  ota.progress = 100;
  snprintf(ota.status, sizeof(ota.status), "Установлено");
  boot_guard_clear();
  delay(500);
  ESP.restart();
  return true;
}

}  // namespace tablo
