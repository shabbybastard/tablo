#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/app_state.h"

namespace tablo {
namespace ui {

struct SdImportModal {
  lv_obj_t* overlay = nullptr;
  lv_obj_t* panel = nullptr;
  lv_obj_t* icon = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* file = nullptr;
  lv_obj_t* status = nullptr;
  lv_obj_t* apply_button = nullptr;
  lv_obj_t* later_label = nullptr;
  bool visible = false;
  bool applying = false;
  bool invalid = false;
  uint32_t event_id = 0;
};

void sd_import_modal_create(SdImportModal& modal);
void sd_import_modal_show(SdImportModal& modal, const SdImportInfo& info);
void sd_import_modal_hide(SdImportModal& modal);
void sd_import_modal_update(SdImportModal& modal, const SdImportInfo& info);

}  // namespace ui
}  // namespace tablo
