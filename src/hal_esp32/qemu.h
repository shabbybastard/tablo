#pragma once

namespace tablo {

/**
 * True when the firmware runs inside the Espressif QEMU `esp32s3` machine.
 *
 * QEMU stores the "QEMU" signature (0x51454D55) in the APB_CTRL register
 * window at offset 0x3F8. The register is reserved on real silicon and reads
 * as zero there, so the check is safe on hardware.
 */
bool qemu_detected();

}  // namespace tablo
