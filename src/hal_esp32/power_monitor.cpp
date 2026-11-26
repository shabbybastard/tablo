#include "power_monitor.h"

#include <Arduino.h>
#include <Wire.h>

#include "core/state_store.h"
#include "log_ring.h"
#include "qemu.h"

namespace tablo {
namespace {
// The board has no built-in voltage/current sensor. An external INA219 on the
// GT911 I2C bus (SDA 19, SCL 45) with the common 0.1 ohm shunt is supported.
constexpr uint8_t kAddress = 0x40;
constexpr float kShuntOhm = 0.1f;

bool s_present = false;
uint32_t s_last_poll_ms = 0;
uint32_t s_next_detect_ms = 0;

bool write_reg(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>(value >> 8));
  Wire.write(static_cast<uint8_t>(value & 0xFF));
  return Wire.endTransmission() == 0;
}

bool read_reg(uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(kAddress), 2) != 2) {
    return false;
  }
  const uint8_t hi = static_cast<uint8_t>(Wire.read());
  const uint8_t lo = static_cast<uint8_t>(Wire.read());
  value = static_cast<uint16_t>((hi << 8) | lo);
  return true;
}
}  // namespace

void power_monitor_begin() {
  if (qemu_detected()) {
    return;
  }
  s_present = false;
  s_last_poll_ms = 0;
  s_next_detect_ms = 0;
}

void power_monitor_poll() {
  const uint32_t now = millis();
  if (now - s_last_poll_ms < 2000U) {
    return;
  }
  s_last_poll_ms = now;
  if (qemu_detected()) {
    return;
  }

  if (!s_present) {
    if (now < s_next_detect_ms) {
      return;
    }
    Wire.beginTransmission(kAddress);
    if (Wire.endTransmission() != 0 ||
        !write_reg(0x00, 0x399F)) {  // 32 V, +/-320 mV, 12-bit, continuous
      s_next_detect_ms = now + 30000U;
      return;
    }
    s_present = true;
    LOGI("power: INA219 at 0x%02X, shunt %.2f ohm", kAddress,
         static_cast<double>(kShuntOhm));
  }

  uint16_t bus_raw = 0;
  uint16_t shunt_raw = 0;
  if (!read_reg(0x02, bus_raw) || !read_reg(0x01, shunt_raw)) {
    s_present = false;
    s_next_detect_ms = now + 30000U;
    g_state.update_power(PowerInfo{});
    LOGW("power: INA219 stopped responding");
    return;
  }

  PowerInfo power;
  power.voltage_v = static_cast<float>(bus_raw >> 3) * 0.004f;
  const int16_t shunt = static_cast<int16_t>(shunt_raw);
  const float current = static_cast<float>(shunt) * 10e-6f / kShuntOhm;
  power.current_a = current < 0 ? -current : current;
  power.power_w = power.voltage_v * power.current_a;
  power.valid = true;
  g_state.update_power(power);
}

}  // namespace tablo
