#pragma once

#include <cstdint>

namespace tablo {
namespace board {

// Guition ESP32-4848S040 / ST7701S 480x480 RGB panel.
// Verified against:
//   FigueiredoStable/LovyanGFX-GUITION-ESP32-4848S040
//   sand1812/ESP32-4848S040
//   aquaElectronics/esp32-4848s040-st7701
//   PrayerfulDrop/ESPHome-Guition-ESP32-S3-4848S040
// Do not change these without an explicit hardware request.
constexpr int LCD_DE = 18;
constexpr int LCD_VSYNC = 17;
constexpr int LCD_HSYNC = 16;
constexpr int LCD_PCLK = 21;

constexpr int LCD_R0 = 11;
constexpr int LCD_R1 = 12;
constexpr int LCD_R2 = 13;
constexpr int LCD_R3 = 14;
constexpr int LCD_R4 = 0;

constexpr int LCD_G0 = 8;
constexpr int LCD_G1 = 20;
constexpr int LCD_G2 = 3;
constexpr int LCD_G3 = 46;
constexpr int LCD_G4 = 9;
constexpr int LCD_G5 = 10;

constexpr int LCD_B0 = 4;
constexpr int LCD_B1 = 5;
constexpr int LCD_B2 = 6;
constexpr int LCD_B3 = 7;
constexpr int LCD_B4 = 15;

// 3-wire SPI used only for ST7701 initialization.
constexpr int LCD_SPI_CS = 39;
constexpr int LCD_SPI_SCK = 48;
constexpr int LCD_SPI_MOSI = 47;

// Backlight PWM.
constexpr int LCD_BACKLIGHT = 38;
constexpr int LCD_BACKLIGHT_HZ = 1000;
constexpr int LCD_BACKLIGHT_BITS = 10;

// GT911 capacitive touch.
constexpr int TOUCH_SDA = 19;
constexpr int TOUCH_SCL = 45;
constexpr int TOUCH_I2C_HZ = 100000;
constexpr uint8_t TOUCH_ADDR_PRIMARY = 0x5D;
constexpr uint8_t TOUCH_ADDR_ALT = 0x14;

// microSD (TF) slot. SPI mode, FAT32. MOSI/SCK are shared with the
// software-SPI ST7701 init bus, which is used only once during display_begin().
// Sources: homeding ESP32-4848S040 panel notes and northernman54 pin table.
// Verify against the actual board revision before changing.
constexpr int SD_CS = 42;
constexpr int SD_MISO = 41;
constexpr int SD_MOSI = 47;
constexpr int SD_SCK = 48;
constexpr uint32_t SD_SPI_HZ = 4000000;

constexpr int LCD_WIDTH = 480;
constexpr int LCD_HEIGHT = 480;

// 12 MHz is the value used by the proven ESPHome/Arduino_GFX configs for this
// panel. Bounce buffer lives in PSRAM and prevents row shifts under DMA load.
constexpr int LCD_PCLK_HZ = 12000000;
constexpr int LCD_BOUNCE_PX = LCD_WIDTH * 16;

}  // namespace board
}  // namespace tablo
