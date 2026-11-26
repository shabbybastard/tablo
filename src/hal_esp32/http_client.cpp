#include "http_client.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "core/version.h"
#include "log_ring.h"

namespace tablo {
namespace {

/**
 * Sink for HTTPClient::writeToStream(). Decoding the body through the library
 * handles both Content-Length and chunked responses (Open-Meteo sends chunked),
 * while the limit keeps a bad URL from exhausting the heap.
 */
class LimitedBodyStream : public Stream {
 public:
  explicit LimitedBodyStream(size_t limit) : limit_(limit) {
    body_.reserve(limit_ < 4096 ? limit_ : 4096);
  }

  size_t write(uint8_t byte) override { return write(&byte, 1); }

  size_t write(const uint8_t* data, size_t size) override {
    // Returning 0 makes HTTPClient::writeToStreamDataBlock abort with a short
    // write instead of draining a slow or endless chunked response.
    if (truncated_ || body_.length() >= limit_ ||
        size > limit_ - body_.length()) {
      truncated_ = true;
      return 0;
    }
    body_.concat(reinterpret_cast<const char*>(data), size);
    return size;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

  const String& body() const { return body_; }
  bool truncated() const { return truncated_; }

 private:
  String body_;
  size_t limit_;
  bool truncated_ = false;
};

}  // namespace

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
    char tls_error[96] = {};
    if (url.startsWith("https://")) {
      secure.lastError(tls_error, sizeof(tls_error));
    }
    LOGW("http: GET %s -> %d (%s) tls=%s", url.c_str(), status,
         HTTPClient::errorToString(status).c_str(), tls_error);
    http.end();
    stream_client->stop();
    return false;
  }

  const int announced = http.getSize();
  const size_t limit = max_body ? max_body : 16384;
  LimitedBodyStream sink(limit);
  const int written = http.writeToStream(&sink);
  const bool complete =
      written >= 0 && !sink.truncated() &&
      (announced <= 0 || static_cast<size_t>(written) ==
                             static_cast<size_t>(announced));
  out.body = sink.body();
  if (!complete) {
    LOGW("http: %s status=%d read=%d expected=%d got=%u%s", url.c_str(),
         status, written, announced, static_cast<unsigned>(out.body.length()),
         sink.truncated() ? " (truncated)" : "");
  }
  http.end();
  stream_client->stop();
  delay(40);
  return complete;
}

}  // namespace tablo
