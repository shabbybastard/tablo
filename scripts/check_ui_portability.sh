#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if grep -RInE '#include[[:space:]]*[<"](Arduino|WiFi|ESPmDNS|Preferences|WebServer|Update|NetworkClient|esp_[^"]*|driver/|freertos/)' src/ui src/core; then
  echo "UI/core portability check failed: Arduino/ESP-IDF include found" >&2
  exit 1
fi

if grep -RInE '\b(esp_|xTaskCreate|vTaskDelay|millis\(|delay\()' src/ui src/core; then
  echo "UI/core portability check failed: platform call found" >&2
  exit 1
fi

echo "UI/core portability check: OK"
