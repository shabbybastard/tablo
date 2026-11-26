#include <emscripten.h>
#include <lvgl.h>

#include "core/app_state.h"
#include "core/state_store.h"
#include "hal_wasm/mock_provider.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "ui/ui.h"

namespace {

constexpr uint32_t kUiRefreshMs = 250;

uint32_t tick_ms() {
  return static_cast<uint32_t>(emscripten_get_now());
}

uint32_t last_ui_refresh_ms = 0;
lv_indev_t* g_pointer = nullptr;
int g_pointer_x = 0;
int g_pointer_y = 0;
bool g_pointer_down = false;

void pointer_read_cb(lv_indev_t*, lv_indev_data_t* data) {
  data->point.x = static_cast<int32_t>(g_pointer_x);
  data->point.y = static_cast<int32_t>(g_pointer_y);
  data->state = g_pointer_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

EM_JS(void, tablo_install_pointer_bridge, (), {
  if (typeof document === 'undefined' || window.__tabloPointerBridge) return;
  window.__tabloPointerBridge = true;

  var canvas = Module.canvas || document.getElementById('screen');
  if (!canvas) return;
  var pressed = false;

  function coords(event) {
    var rect = canvas.getBoundingClientRect();
    var w = canvas.width || 480;
    var h = canvas.height || 480;
    var x = Math.round((event.clientX - rect.left) * w / Math.max(1, rect.width));
    var y = Math.round((event.clientY - rect.top) * h / Math.max(1, rect.height));
    return [Math.max(0, Math.min(w - 1, x)), Math.max(0, Math.min(h - 1, y))];
  }

  function send(event, down) {
    pressed = down;
    var p = coords(event);
    if (typeof Module._tablo_preview_pointer === 'function') {
      Module._tablo_preview_pointer(p[0], p[1], down ? 1 : 0);
    }
    if (event.cancelable) event.preventDefault();
  }

  canvas.addEventListener('pointerdown', function (event) {
    if (canvas.setPointerCapture) {
      try { canvas.setPointerCapture(event.pointerId); } catch (e) {}
    }
    send(event, true);
  }, {passive: false});

  function drag(event) {
    if (pressed || (event.buttons & 1)) send(event, true);
  }
  canvas.addEventListener('pointermove', drag, {passive: false});
  // CDP/headless and some desktop browsers emit mousemove without a
  // corresponding pointermove while dragging; keep the drag path reliable.
  canvas.addEventListener('mousemove', drag, {passive: false});

  window.addEventListener('pointerup', function (event) {
    send(event, false);
  }, {passive: false});

  window.addEventListener('mouseup', function (event) {
    if (pressed) send(event, false);
  }, {passive: false});

  canvas.addEventListener('pointercancel', function (event) {
    send(event, false);
  }, {passive: false});

  canvas.addEventListener('contextmenu', function (event) {
    event.preventDefault();
  });
});

void refresh_ui() {
  tablo::AppState state = tablo::g_state.snapshot();
  tablo::mock_provider().refresh(state);
  tablo::g_state.set(state);
  tablo::ui_update(tablo::g_state.snapshot());
}

void main_loop() {
  lv_timer_handler();

  const uint32_t now = tick_ms();
  if (now - last_ui_refresh_ms >= kUiRefreshMs) {
    last_ui_refresh_ms = now;
    refresh_ui();
  }
}

}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void tablo_preview_pointer(int x, int y, int pressed) {
  g_pointer_x = x;
  g_pointer_y = y;
  g_pointer_down = pressed != 0;
  if (g_pointer != nullptr) {
    lv_indev_read(g_pointer);
  }
}

int main() {
  lv_init();
  lv_tick_set_cb(tick_ms);

  lv_display_t* display = lv_sdl_window_create(480, 480);
  if (display == nullptr) {
    return 1;
  }
  lv_display_set_default(display);
  lv_sdl_window_set_title(display, "tablo 480x480");
  // The SDL driver installs SDL_GetTicks(); keep the browser clock explicitly.
  lv_tick_set_cb(tick_ms);

  tablo::mock_provider().begin();
  tablo::ui_init();
  tablo::ui_update(tablo::g_state.snapshot());

  g_pointer = lv_indev_create();
  lv_indev_set_type(g_pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(g_pointer, pointer_read_cb);
  lv_indev_set_display(g_pointer, display);
  lv_indev_set_mode(g_pointer, LV_INDEV_MODE_EVENT);
  tablo_install_pointer_bridge();

  last_ui_refresh_ms = tick_ms();
  emscripten_set_main_loop(main_loop, 0, 1);
  return 0;
}
