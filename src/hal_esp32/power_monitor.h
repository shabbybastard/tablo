#pragma once

namespace tablo {

/** Probe the optional INA219 power monitor on the shared touch I2C bus. */
void power_monitor_begin();
void power_monitor_poll();

}  // namespace tablo
