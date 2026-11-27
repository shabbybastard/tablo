#pragma once

#include <cstdint>

#include "core/date_format.h"
#include "core/schedule.h"

namespace tablo {

/** One-shot actions requested by the portable UI. */
enum class PlatformAction : uint8_t {
  SetupChooseWifi = 0,
  SetupChooseSd,
  SetupBack,
  SdImportApply,
  SdImportDismiss,
};

/**
 * Hardware actions requested by the portable UI. Each HAL links exactly one
 * implementation of platform().
 */
class IPlatform {
 public:
  virtual ~IPlatform() = default;
  virtual void setBrightness(uint8_t percent) = 0;
  virtual void setNightMode(bool on) = 0;
  virtual void setNightSchedule(const NightSchedule& schedule) = 0;
  virtual void setTimezone(const char* timezone) = 0;
  virtual void setDateFormat(DateFormat format) = 0;
  virtual void setWeatherEnabled(bool enabled) = 0;
  virtual void setExternalConfig(bool enabled, const char* ip,
                                 const char* token) = 0;
  virtual void requestExternalRefresh() = 0;
  virtual void requestWifiScan() = 0;
  virtual void connectWifi(const char* ssid, const char* password) = 0;
  virtual void checkForUpdates() = 0;
  virtual void installUpdate() = 0;
  virtual void reboot() = 0;
  virtual void requestAction(PlatformAction action) = 0;
};

IPlatform& platform();

}  // namespace tablo
