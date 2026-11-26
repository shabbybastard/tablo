#!/usr/bin/env bash
# Boot the real ESP32-S3 firmware in Espressif QEMU and validate UART output.
#
# The binary is the same one flashed onto the board: bootloader + partition
# table + OTA app merged into a 16 MB flash image. QEMU does not model Wi-Fi or
# the GT911, so the firmware detects the emulator and continues offline with a
# read-only UI instead of blocking in the setup portal.
#
# Note: `-global driver=ssi_psram,property=is_octal,value=true` makes QEMU's
# PSRAM model octal, matching the N16R8 board (qio_opi). Without it the RGB
# panel has no framebuffer memory and the app correctly disables the UI.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export PATH="${HOME}/.local/bin:${HOME}/.platformio/penv/bin:${PATH}"

BUILD_DIR="${BUILD_DIR:-${ROOT}/.pio/build/esp32s3}"
FLASH_IMAGE="${BUILD_DIR}/qemu_flash.bin"
UART_LOG="${BUILD_DIR}/qemu_uart.log"
QEMU_TIMEOUT="${QEMU_TIMEOUT:-12}"
QEMU_PSRAM_MB="${QEMU_PSRAM_MB:-8}"
QEMU_BIN="${QEMU_BIN:-qemu-system-xtensa}"
PIO="${PIO:-pio}"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

require_line() {
  if ! grep -Fq -- "$1" "$CLEAN_LOG"; then
    echo "--- UART tail ---" >&2
    tail -40 "$CLEAN_LOG" >&2 || true
    fail "UART marker not found: $1"
  fi
  echo "  ok: $1"
}

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
  fail "qemu-system-xtensa is not installed (run scripts/install_qemu.sh)"
fi
if ! "$QEMU_BIN" -machine help 2>/dev/null | grep -qw esp32s3; then
  fail "$QEMU_BIN does not support the esp32s3 machine"
fi

echo "==> Building firmware"
"$PIO" run -e esp32s3

for artifact in bootloader.bin partitions.bin firmware.bin; do
  [[ -s "${BUILD_DIR}/${artifact}" ]] || fail "missing ${BUILD_DIR}/${artifact}"
done

# esptool v5 renamed merge_bin to merge-bin but still accepts the old spelling.
ESPTOOL=()
if [[ -n "${ESPTOOL_BIN:-}" ]]; then
  ESPTOOL=("${ESPTOOL_BIN}")
elif [[ -x "${HOME}/.platformio/penv/bin/esptool.py" ]]; then
  ESPTOOL=("${HOME}/.platformio/penv/bin/esptool.py")
elif command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL=("$(command -v esptool.py)")
elif command -v esptool >/dev/null 2>&1; then
  ESPTOOL=("$(command -v esptool)")
else
  ESPTOOL=(python3 -m esptool)
fi

MERGE_CMD="merge-bin"
if ! "${ESPTOOL[@]}" --help 2>&1 | grep -q "merge-bin"; then
  MERGE_CMD="merge_bin"
fi

BOOT_APP0="${HOME}/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
if [[ ! -f "$BOOT_APP0" ]]; then
  BOOT_APP0="$(find "${HOME}/.platformio/packages/framework-arduinoespressif32" \
    -path '*/tools/partitions/boot_app0.bin' -print -quit 2>/dev/null || true)"
fi
if [[ -z "$BOOT_APP0" || ! -f "$BOOT_APP0" ]]; then
  fail "boot_app0.bin not found; run: pio pkg install -e esp32s3"
fi

MERGE_ARGS=(
  --chip esp32s3 "$MERGE_CMD"
  -o "$FLASH_IMAGE"
  --flash_mode qio --flash_freq 80m --flash_size 16MB
  0x0 "${BUILD_DIR}/bootloader.bin"
  0x8000 "${BUILD_DIR}/partitions.bin"
  0xe000 "$BOOT_APP0"
  0x10000 "${BUILD_DIR}/firmware.bin"
)

echo "==> Merging flash image (16 MB)"
"${ESPTOOL[@]}" "${MERGE_ARGS[@]}"
truncate -s 16M "$FLASH_IMAGE"

echo "==> Running QEMU for ${QEMU_TIMEOUT}s"
set +e
timeout "${QEMU_TIMEOUT}s" "$QEMU_BIN" \
  -nographic \
  -machine esp32s3 \
  -m "${QEMU_PSRAM_MB}M" \
  -global driver=ssi_psram,property=is_octal,value=true \
  -drive "file=${FLASH_IMAGE},if=mtd,format=raw" \
  >"$UART_LOG" 2>&1
QEMU_STATUS=$?
set -e

# timeout(1) is the expected way to stop the emulator.
if [[ "$QEMU_STATUS" -ne 0 && "$QEMU_STATUS" -ne 124 ]]; then
  tail -60 "$UART_LOG" >&2 || true
  fail "QEMU exited with status ${QEMU_STATUS}"
fi

CLEAN_LOG="${UART_LOG}.clean"
# Drop ANSI colour sequences and CR characters emitted by the ROM/QEMU.
sed -E $'s/\x1B\\[[0-9;]*[mK]//g; s/\r$//' "$UART_LOG" >"$CLEAN_LOG"

echo "==> Checking UART output"
require_line "boot: attempt 1"
require_line "nvs: namespace 'tablo' ready"
require_line "boot: running from ota_0"
require_line "boot: PSRAM ${QEMU_PSRAM_MB} MB"
require_line "boot: Espressif QEMU esp32s3 machine detected"
require_line "[display] ST7701S init 480x480"
require_line "[display] ready"
require_line "[display] LVGL 480x480"
require_line "[touch] QEMU: GT911 is not modeled, UI is read-only"
require_line "wifi: QEMU detected, Wi-Fi is unavailable, continuing offline"
require_line "net: worker started on core 0"
require_line "lvgl: UI ready 480x480"
require_line "boot: services started"

if grep -Eiq "Guru Meditation Error|panic|abort\(\)|assert failed|Backtrace:|Rebooting\.\.\." "$CLEAN_LOG"; then
  grep -Ein "Guru Meditation Error|panic|abort\(\)|assert failed|Backtrace:|Rebooting\.\.\." "$CLEAN_LOG" >&2 || true
  fail "crash or reboot signature found in UART log"
fi

ROM_COUNT="$(grep -c "ESP-ROM:esp32s3" "$CLEAN_LOG" || true)"
BOOT_COUNT="$(grep -cE "I tablo .* boot$" "$CLEAN_LOG" || true)"
if [[ "$ROM_COUNT" -ne 1 || "$BOOT_COUNT" -ne 1 ]]; then
  fail "firmware rebooted (ROM banners: ${ROM_COUNT}, app boots: ${BOOT_COUNT})"
fi

echo "==> UART fragment"
grep -E "ESP-ROM|boot: attempt|nvs:|boot: running|PSRAM|QEMU esp32s3|display:|touch:|wifi:|web:|net: worker|lvgl: UI|boot: services" \
  "$CLEAN_LOG" | tail -25

echo "QEMU firmware smoke test: OK (UART: ${UART_LOG})"
