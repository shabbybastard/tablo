#include "touch.h"

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

#include "board_pins.h"
#include "qemu.h"

namespace tablo {
namespace {

// GT911 register map (16-bit register addresses, big-endian on the wire).
constexpr uint16_t kRegProductId = 0x8140;
constexpr uint16_t kRegStatus = 0x814E;
constexpr uint16_t kRegPoint1 = 0x8150;
constexpr uint8_t kStatusBufferReady = 0x80;
constexpr uint8_t kStatusTouchCountMask = 0x0F;

bool s_ready = false;
uint8_t s_addr = 0;
uint16_t s_x = 0;
uint16_t s_y = 0;
bool s_pressed = false;
lv_indev_t* s_indev = nullptr;

bool gt911_write_reg16(uint8_t addr, uint16_t reg, const uint8_t* data,
                       size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  for (size_t i = 0; i < len; ++i) {
    Wire.write(data[i]);
  }
  return Wire.endTransmission() == 0;
}

bool gt911_read_reg16(uint8_t addr, uint16_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t received = Wire.requestFrom(addr, static_cast<uint8_t>(len));
  if (received != len) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    data[i] = Wire.read();
  }
  return true;
}

bool gt911_probe(uint8_t addr) {
  uint8_t id[4] = {0};
  if (!gt911_read_reg16(addr, kRegProductId, id, sizeof(id))) {
    return false;
  }

  Serial.printf("[touch] probe 0x%02X -> id %02X %02X %02X %02X '%c%c%c%c'\n",
                static_cast<unsigned>(addr), static_cast<unsigned>(id[0]),
                static_cast<unsigned>(id[1]), static_cast<unsigned>(id[2]),
                static_cast<unsigned>(id[3]),
                id[0] >= 0x20 && id[0] < 0x7F ? id[0] : '.',
                id[1] >= 0x20 && id[1] < 0x7F ? id[1] : '.',
                id[2] >= 0x20 && id[2] < 0x7F ? id[2] : '.',
                id[3] >= 0x20 && id[3] < 0x7F ? id[3] : '.');

  // A floating or absent bus usually reads all 0x00 or all 0xFF. Any real
  // response means something acknowledged the GT911 register layout.
  bool all_zero = true;
  bool all_ff = true;
  for (size_t i = 0; i < sizeof(id); ++i) {
    if (id[i] != 0x00) {
      all_zero = false;
    }
    if (id[i] != 0xFF) {
      all_ff = false;
    }
  }
  return !all_zero && !all_ff;
}

uint16_t clamp_coord(uint16_t value, int max_value) {
  return value <= static_cast<uint16_t>(max_value)
             ? value
             : static_cast<uint16_t>(max_value);
}

void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;

  if (s_ready) {
    uint8_t status = 0;
    if (gt911_read_reg16(s_addr, kRegStatus, &status, 1) &&
        (status & kStatusBufferReady) != 0) {
      uint8_t point[4] = {0};
      if (gt911_read_reg16(s_addr, kRegPoint1, point, sizeof(point))) {
        const uint16_t x =
            static_cast<uint16_t>(point[0]) |
            (static_cast<uint16_t>(point[1]) << 8);
        const uint16_t y =
            static_cast<uint16_t>(point[2]) |
            (static_cast<uint16_t>(point[3]) << 8);
        s_pressed = (status & kStatusTouchCountMask) != 0;
        if (s_pressed) {
          s_x = clamp_coord(x, board::LCD_WIDTH - 1);
          s_y = clamp_coord(y, board::LCD_HEIGHT - 1);
        }
      }

      // Acknowledge the buffer so the controller reports the next frame.
      const uint8_t clear = 0;
      gt911_write_reg16(s_addr, kRegStatus, &clear, 1);
    }
  }

  data->point.x = s_x;
  data->point.y = s_y;
  data->state =
      s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

}  // namespace

bool touch_begin() {
  if (qemu_detected()) {
    // QEMU does not model the GT911 or the I2C bus. Probing it can stall the
    // I2C driver, so skip it and keep the UI read-only.
    Serial.println("[touch] QEMU: GT911 is not modeled, UI is read-only");
    return false;
  }

  if (!Wire.begin(board::TOUCH_SDA, board::TOUCH_SCL, board::TOUCH_I2C_HZ)) {
    Serial.printf("[touch] I2C init failed (SDA %d, SCL %d), UI is read-only\n",
                  board::TOUCH_SDA, board::TOUCH_SCL);
    return false;
  }
  Wire.setTimeOut(50);

  // No reset/interrupt line is wired on this panel; the controller is already
  // running with one of these two addresses, so just probe both.
  delay(20);

  const uint8_t candidates[2] = {board::TOUCH_ADDR_PRIMARY,
                                 board::TOUCH_ADDR_ALT};
  for (const uint8_t addr : candidates) {
    if (gt911_probe(addr)) {
      s_addr = addr;
      s_ready = true;
      break;
    }
  }

  if (!s_ready) {
    Serial.println("[touch] GT911 not found at 0x5D or 0x14");
    return false;
  }

  // Drop any stale status left over from before the firmware started.
  const uint8_t clear = 0;
  gt911_write_reg16(s_addr, kRegStatus, &clear, 1);

  Serial.printf("[touch] GT911 ready at 0x%02X (SDA %d, SCL %d, %d Hz)\n",
                static_cast<unsigned>(s_addr), board::TOUCH_SDA,
                board::TOUCH_SCL, board::TOUCH_I2C_HZ);
  return true;
}

lv_indev_t* touch_lvgl_create() {
  if (s_indev != nullptr) {
    return s_indev;
  }

  s_indev = lv_indev_create();
  if (s_indev == nullptr) {
    Serial.println("[touch] lv_indev_create failed");
    return nullptr;
  }

  lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(s_indev, touch_read_cb);
  // Keep LVGL's default 30 ms read timer (LV_DEF_REFR_PERIOD).

  Serial.printf("[touch] LVGL pointer indev created (%s)\n",
                s_ready ? "GT911" : "fallback, always released");
  return s_indev;
}

}  // namespace tablo
