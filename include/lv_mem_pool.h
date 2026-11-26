#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** LVGL heap pool: prefer PSRAM, fall back to internal RAM. */
void* tablo_lv_mem_alloc(size_t size);

#ifdef __cplusplus
}
#endif
