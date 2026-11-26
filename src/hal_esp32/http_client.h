#pragma once

#include <Arduino.h>

namespace tablo {

struct HttpResponse {
  int status = -1;
  String body;
};

/**
 * One sequential HTTP(S) request. HTTPS uses the built-in CA bundle (the
 * Arduino core 3.x equivalent of setCACertBundle) and is never parallel.
 */
bool http_get(const String& url, size_t max_body, HttpResponse& out);

}  // namespace tablo
