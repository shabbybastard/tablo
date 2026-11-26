#include "http_client.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "core/version.h"
#include "log_ring.h"

namespace tablo {

bool http_get(const String& url, size_t max_body, HttpResponse& out) {
  out = HttpResponse();
  if (!url.startsWith("http://") && !url.startsWith("https://")) {
    LOGW("http: unsupported url %s", url.c_str());
    return false;
  }

  WiFiClient plain;
  WiFiClientSecure secure;
  WiFiClient* stream_client = nullptr;
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setReuse(false);
  http.setUserAgent(String("tablo/") + kFwVersion);

  bool begun = false;
  if (url.startsWith("https://")) {
    secure.setHandshakeTimeout(8);
    secure.useBuiltinCACertBundle();
    begun = http.begin(secure, url);
    stream_client = &secure;
  } else {
    begun = http.begin(plain, url);
    stream_client = &plain;
  }
  if (!begun) {
    LOGW("http: begin failed for %s", url.c_str());
    return false;
  }

  const int status = http.GET();
  out.status = status;
  if (status <= 0) {
    LOGW("http: GET %s -> %d", url.c_str(), status);
    http.end();
    stream_client->stop();
    return false;
  }

  const int announced = http.getSize();
  const size_t limit = max_body ? max_body : 16384;
  String body;
  body.reserve(announced > 0 && static_cast<size_t>(announced) < limit ? announced + 1 : limit / 2);
  Stream* stream = http.getStreamPtr();
  uint8_t buf[512];
  while (stream && (http.connected() || stream->available()) && body.length() < limit) {
    const size_t want = min(sizeof(buf), limit - body.length());
    const int n = stream->readBytes(buf, want);
    if (n <= 0) {
      if (!http.connected()) break;
      delay(2);
      continue;
    }
    body.concat(reinterpret_cast<const char*>(buf), n);
  }
  out.body = body;
  http.end();
  stream_client->stop();
  delay(40);
  return true;
}

}  // namespace tablo
