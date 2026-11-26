#include "qemu.h"

#include <soc/soc.h>

namespace tablo {
namespace {
// Written by Espressif QEMU in hw/xtensa/esp32s3.c (RGB_QEMU_ORIGIN).
constexpr uintptr_t kQemuSignatureReg = DR_REG_APB_CTRL_BASE + 0x3F8;
constexpr uint32_t kQemuSignature = 0x51454D55;  // "QEMU" in little-endian order
}  // namespace

bool qemu_detected() {
  static const bool detected =
      *reinterpret_cast<volatile const uint32_t*>(kQemuSignatureReg) ==
      kQemuSignature;
  return detected;
}

}  // namespace tablo
