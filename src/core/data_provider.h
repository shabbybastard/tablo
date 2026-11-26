#pragma once

#include "app_state.h"
#include "settings.h"

namespace tablo {

/**
 * Blocking data source. `refresh` is called from the network worker only, so
 * implementations may perform sequential HTTP/TLS requests. UI code must never
 * call it directly.
 */
class IDataProvider {
 public:
  virtual ~IDataProvider() = default;
  virtual void begin(const Settings& settings) = 0;
  virtual bool refresh(AppState& state) = 0;
  virtual void requestUpdateCheck() {}
  virtual void requestInstallUpdate() {}
};

}  // namespace tablo
