#include "display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <lvgl.h>

#include <cstring>

#include "board_pins.h"

namespace tablo {
namespace {

// ST7701S init sequence for the GUITION ESP32-4848S040 (panel type
// 4848C080). Copied verbatim from aquaElectronics/esp32-4848s040-st7701
// (src/display.cpp), Copyright (c) 2026 aquaElectronics, MIT License.
static const uint8_t st7701_type4848C080_init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,

    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0D, 0x02,
    WRITE_C8_D16, 0xC2, 0x31, 0x05,
    WRITE_C8_D8, 0xCD, 0x00,  // 0x08

    WRITE_COMMAND_8, 0xB0,  // Positive Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x18, 0x0E,
    0x11, 0x06, 0x07, 0x08,
    0x07, 0x22, 0x04, 0x12,
    0x0F, 0xAA, 0x31, 0x18,

    WRITE_COMMAND_8, 0xB1,  // Negative Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x19, 0x0E,
    0x12, 0x07, 0x08, 0x08,
    0x08, 0x22, 0x04, 0x11,
    0x11, 0xA9, 0x32, 0x18,

    // PAGE1
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,

    WRITE_C8_D8, 0xB0, 0x60,  // Vop=4.7375v
    WRITE_C8_D8, 0xB1, 0x32,  // VCOM=32
    WRITE_C8_D8, 0xB2, 0x07,  // VGH=15v
    WRITE_C8_D8, 0xB3, 0x80,
    WRITE_C8_D8, 0xB5, 0x49,  // VGL=-10.17v
    WRITE_C8_D8, 0xB7, 0x85,
    WRITE_C8_D8, 0xB8, 0x21,  // AVDD=6.6 & AVCL=-4.6
    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,

    WRITE_COMMAND_8, 0xE0,
    WRITE_BYTES, 3, 0x00, 0x1B, 0x02,

    WRITE_COMMAND_8, 0xE1,
    WRITE_BYTES, 11,
    0x08, 0xA0, 0x00, 0x00,
    0x07, 0xA0, 0x00, 0x00,
    0x00, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE2,
    WRITE_BYTES, 12,
    0x11, 0x11, 0x44, 0x44,
    0xED, 0xA0, 0x00, 0x00,
    0xEC, 0xA0, 0x00, 0x00,

    WRITE_COMMAND_8, 0xE3,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE4, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 16,
    0x0A, 0xE9, 0xD8, 0xA0,
    0x0C, 0xEB, 0xD8, 0xA0,
    0x0E, 0xED, 0xD8, 0xA0,
    0x10, 0xEF, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xE6,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE7, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE8,
    WRITE_BYTES, 16,
    0x09, 0xE8, 0xD8, 0xA0,
    0x0B, 0xEA, 0xD8, 0xA0,
    0x0D, 0xEC, 0xD8, 0xA0,
    0x0F, 0xEE, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xEB,
    WRITE_BYTES, 7,
    0x02, 0x00, 0xE4, 0xE4,
    0x88, 0x00, 0x40,

    WRITE_C8_D16, 0xEC, 0x3C, 0x00,

    WRITE_COMMAND_8, 0xED,
    WRITE_BYTES, 16,
    0xAB, 0x89, 0x76, 0x54,
    0x02, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x20,
    0x45, 0x67, 0x98, 0xBA,

    //-----------VAP & VAN---------------
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,

    WRITE_C8_D8, 0xE5, 0xE4,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x00,

    WRITE_C8_D8, 0x3A, 0x60,  // 0x70 RGB888, 0x60 RGB666, 0x50 RGB565

    // WRITE_COMMAND_8, 0x21, 0x00,
    // END_WRITE,

    DELAY, 10,
    WRITE_COMMAND_8, 0x11,  // Sleep Out
    END_WRITE,

    DELAY, 120,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,  // Display On

    END_WRITE};

bool s_begun = false;
Arduino_DataBus* s_bus = nullptr;
Arduino_ESP32RGBPanel* s_rgb_panel = nullptr;
Arduino_RGB_Display* s_gfx = nullptr;
lv_display_t* s_lv_display = nullptr;
uint16_t* s_draw_buf = nullptr;
bool s_lvgl_active = false;

void draw_message_line(const char* text, int y) {
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  constexpr int kCharWidth = 12;  // 6 px font at 2x scale.
  const int width = static_cast<int>(strlen(text)) * kCharWidth;
  int x = (board::LCD_WIDTH - width) / 2;
  if (x < 0) {
    x = 0;
  }
  s_gfx->setCursor(x, y);
  s_gfx->print(text);
}

void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area,
                   uint8_t* px_map) {
  if (s_gfx != nullptr) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    s_gfx->draw16bitRGBBitmap(area->x1, area->y1,
                              reinterpret_cast<uint16_t*>(px_map), w, h);
  }
  lv_display_flush_ready(disp);
}

}  // namespace

bool display_begin() {
  if (s_begun) {
    return true;
  }

  Serial.printf("[display] ST7701S init %dx%d, PCLK %d Hz\n", board::LCD_WIDTH,
                board::LCD_HEIGHT, board::LCD_PCLK_HZ);

  // Arduino_ESP32RGBPanel allocates its frame buffers in PSRAM and aborts via
  // ESP_ERROR_CHECK when none is available (for example in QEMU without the
  // octal PSRAM model). Treat that as a recoverable panel failure so the rest
  // of the firmware keeps running, exactly like a missing physical panel.
  if (!esp_psram_is_initialized()) {
    Serial.println(
        "[display] PSRAM is not initialized, skipping RGB panel (UI disabled)");
    return false;
  }

  if (s_bus == nullptr) {
    s_bus = new Arduino_SWSPI(GFX_NOT_DEFINED, board::LCD_SPI_CS,
                              board::LCD_SPI_SCK, board::LCD_SPI_MOSI,
                              GFX_NOT_DEFINED);
  }
  if (s_rgb_panel == nullptr) {
    s_rgb_panel = new Arduino_ESP32RGBPanel(
        board::LCD_DE, board::LCD_VSYNC, board::LCD_HSYNC, board::LCD_PCLK,
        board::LCD_R0, board::LCD_R1, board::LCD_R2, board::LCD_R3,
        board::LCD_R4, board::LCD_G0, board::LCD_G1, board::LCD_G2,
        board::LCD_G3, board::LCD_G4, board::LCD_G5, board::LCD_B0,
        board::LCD_B1, board::LCD_B2, board::LCD_B3, board::LCD_B4,
        1 /* hsync_polarity */, 10 /* hsync_front_porch */,
        8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
        1 /* vsync_polarity */, 10 /* vsync_front_porch */,
        8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
        0 /* pclk_active_neg */, board::LCD_PCLK_HZ, false /* useBigEndian */,
        0 /* de_idle_high */, 0 /* pclk_idle_high */,
        board::LCD_BOUNCE_PX /* bounce_buffer_size_px */);
  }
  if (s_gfx == nullptr) {
    s_gfx = new Arduino_RGB_Display(
        board::LCD_WIDTH, board::LCD_HEIGHT, s_rgb_panel, 0 /* rotation */,
        true /* auto_flush */, s_bus, GFX_NOT_DEFINED /* RST */,
        st7701_type4848C080_init_operations,
        sizeof(st7701_type4848C080_init_operations));
  }

  if (s_bus == nullptr || s_rgb_panel == nullptr || s_gfx == nullptr) {
    Serial.println("[display] allocation failed");
    return false;
  }

  if (!s_gfx->begin()) {
    Serial.println("[display] begin failed");
    return false;
  }

  s_gfx->fillScreen(RGB565_BLACK);
  s_gfx->displayOn();
  s_begun = true;

  Serial.println("[display] ready");
  return true;
}

void display_show_message(const char* line1, const char* line2,
                          const char* line3) {
  if (!s_begun || s_gfx == nullptr || s_lvgl_active) {
    Serial.printf("[display] message: %s %s %s\n", line1 != nullptr ? line1 : "",
                  line2 != nullptr ? line2 : "",
                  line3 != nullptr ? line3 : "");
    return;
  }

  const char* lines[3] = {line1, line2, line3};
  int count = 0;
  for (const char* line : lines) {
    if (line != nullptr && line[0] != '\0') {
      ++count;
    }
  }
  if (count == 0) {
    return;
  }

  s_gfx->fillScreen(RGB565_BLACK);
  s_gfx->setTextColor(RGB565_WHITE);
  s_gfx->setTextSize(2);
  s_gfx->setTextWrap(false);

  constexpr int kLineHeight = 28;
  int y = (board::LCD_HEIGHT - count * kLineHeight) / 2;
  for (const char* line : lines) {
    if (line != nullptr && line[0] != '\0') {
      draw_message_line(line, y);
      y += kLineHeight;
    }
  }
}

lv_display_t* display_lvgl_create() {
  if (s_lv_display != nullptr) {
    return s_lv_display;
  }
  if (!s_begun) {
    Serial.println("[display] display_lvgl_create() before display_begin()");
    return nullptr;
  }

  // One partial buffer: 480 x 16 RGB565 pixels = 15360 bytes, kept in
  // internal DMA-capable RAM. 16 lines leave more internal heap for Wi-Fi,
  // TLS and the web server; LVGL just flushes twice as often.
  constexpr size_t kDrawBufPixels = board::LCD_WIDTH * 16;
  constexpr size_t kDrawBufBytes = kDrawBufPixels * sizeof(uint16_t);
  s_draw_buf = static_cast<uint16_t*>(heap_caps_malloc(
      kDrawBufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  if (s_draw_buf == nullptr) {
    Serial.printf("[display] draw buffer allocation failed (%u bytes)\n",
                  static_cast<unsigned>(kDrawBufBytes));
    return nullptr;
  }

  s_lv_display = lv_display_create(board::LCD_WIDTH, board::LCD_HEIGHT);
  if (s_lv_display == nullptr) {
    Serial.println("[display] lv_display_create failed");
    heap_caps_free(s_draw_buf);
    s_draw_buf = nullptr;
    return nullptr;
  }

  lv_display_set_color_format(s_lv_display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(s_lv_display, lvgl_flush_cb);
  lv_display_set_buffers(s_lv_display, s_draw_buf, nullptr, kDrawBufBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  s_lvgl_active = true;
  Serial.printf("[display] LVGL %dx%d, partial buffer %u bytes (%u lines)\n",
                board::LCD_WIDTH, board::LCD_HEIGHT,
                static_cast<unsigned>(kDrawBufBytes),
                static_cast<unsigned>(kDrawBufPixels / board::LCD_WIDTH));
  return s_lv_display;
}

bool display_lvgl_active() { return s_lvgl_active; }

}  // namespace tablo
