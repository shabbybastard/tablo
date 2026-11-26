#include "render.h"

#include <Arduino.h>
#include <freertos/semphr.h>
#include <lvgl.h>

#include <atomic>

#include "core/state_store.h"
#include "log_ring.h"
#include "ui/ui.h"

namespace tablo {
namespace {
std::atomic<bool> g_render_paused{false};
std::atomic<bool> g_render_started{false};
SemaphoreHandle_t g_render_ack = nullptr;
}  // namespace

bool render_pause() {
  if (!g_render_started.load(std::memory_order_acquire)) {
    return true;  // no LVGL task exists (display init failed)
  }
  if (g_render_ack == nullptr) {
    return false;
  }

  // Drain a stale acknowledgement before announcing the new pause request.
  while (xSemaphoreTake(g_render_ack, 0) == pdTRUE) {
  }
  g_render_paused.store(true, std::memory_order_release);
  if (xSemaphoreTake(g_render_ack, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOGE("render: pause ack timeout, refusing flash operation");
    g_render_paused.store(false, std::memory_order_release);
    return false;
  }
  return true;
}

void render_resume() { g_render_paused.store(false, std::memory_order_release); }

namespace {
void render_task(void*) {
  uint32_t last_ui = 0;
  while (true) {
    if (g_render_paused.load(std::memory_order_acquire)) {
      xSemaphoreGive(g_render_ack);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    lv_timer_handler();
    if (millis() - last_ui > 200) {
      last_ui = millis();
      ui_update(g_state.snapshot());
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
}  // namespace

void render_task_start() {
  if (g_render_ack == nullptr) {
    g_render_ack = xSemaphoreCreateBinary();
  }
  if (g_render_ack == nullptr) {
    LOGE("render: cannot create pause semaphore, task not started");
    return;
  }
  g_render_started.store(true, std::memory_order_release);
  xTaskCreatePinnedToCore(render_task, "lvgl", 8192, nullptr, 2, nullptr, 1);
}

}  // namespace tablo
