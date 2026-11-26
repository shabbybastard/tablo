/**
 * tablo LVGL configuration.
 *
 * One file is shared by the ESP32-S3 firmware and the Emscripten preview.
 * Only the SDL window driver differs; it is compiled exclusively for WASM.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_SIZE (96U * 1024U)
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DPI_DEF 130
#define LV_DEF_REFR_PERIOD 30
#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 0

/*====================
   FONTS
 *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#define LV_USE_FONT_COMPRESSED 0

/*====================
   WIDGETS / THEME
 *====================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_USE_CHART 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_BUTTON 1
#define LV_USE_LABEL 1
#define LV_USE_BAR 1
#define LV_USE_LED 1
#define LV_USE_OBJ_PROPERTY 0

/*====================
   DEMOS / EXAMPLES
 *====================*/
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_MUSIC 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0
#define LV_USE_DEMO_RENDER 0
#define LV_USE_DEMO_MULTILANG 0
#define LV_USE_DEMO_TRANSFORM 0
#define LV_USE_DEMO_FLEX_LAYOUT 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_VECTOR_GRAPHIC 0
#define LV_USE_DEMO_SVG 0

/*====================
   OPTIONAL MODULES
 *====================*/
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_VECTOR_GRAPHIC 0
#define LV_USE_MATRIX 0
#define LV_USE_FLOAT 0
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_LODEPNG 0
#define LV_USE_BARCODE 0
#define LV_USE_FILE_EXPLORER 0
#define LV_USE_FRAGMENT 0
#define LV_USE_IMGFONT 0
#define LV_USE_OBJ_ID 0

/*====================
   PREVIEW-ONLY SDL DRIVER
 *====================*/
#ifdef TABLO_PREVIEW
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT 1
#define LV_SDL_ACCELERATED 0
#define LV_SDL_FULLSCREEN 0
#define LV_SDL_DIRECT_EXIT 1
#define LV_SDL_MOUSEWHEEL_MODE LV_SDL_MOUSEWHEEL_MODE_ENCODER
#else
#define LV_USE_SDL 0
#endif

#endif /* LV_CONF_H */
