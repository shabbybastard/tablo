#include "lv_mem_pool.h"

#include <esp_heap_caps.h>

extern "C" void* tablo_lv_mem_alloc(size_t size) {
  void* memory = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (memory == nullptr) {
    memory = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  return memory;
}
