#!/usr/bin/env bash
# Install the Espressif QEMU fork (XTENSA, esp32s3 machine) into ~/.local.
#
# Usage:
#   scripts/install_qemu.sh
#   QEMU_VERSION=esp-develop-9.2.2-20261109 scripts/install_qemu.sh
#
# Nothing is written outside the user's home directory and no sudo is needed.
set -euo pipefail

QEMU_VERSION="${QEMU_VERSION:-esp-develop-9.2.2-20261109}"
QEMU_ASSET_VERSION="${QEMU_VERSION//-/_}"
INSTALL_DIR="${QEMU_INSTALL_DIR:-${HOME}/.local/opt/qemu-xtensa}"
BIN_DIR="${QEMU_BIN_DIR:-${HOME}/.local/bin}"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "error: this installer supports Linux/WSL2 only" >&2
  exit 1
fi

case "$(uname -m)" in
  x86_64) ARCH="x86_64" ;;
  aarch64|arm64) ARCH="aarch64" ;;
  *)
    echo "error: unsupported architecture: $(uname -m)" >&2
    exit 1
    ;;
esac

if command -v qemu-system-xtensa >/dev/null 2>&1 &&
   qemu-system-xtensa -machine help 2>/dev/null | grep -qw esp32s3; then
  echo "Espressif QEMU is already installed:"
  qemu-system-xtensa --version | head -1
  qemu-system-xtensa -machine help | grep -w esp32s3
  exit 0
fi

ASSET="qemu-xtensa-softmmu-${QEMU_ASSET_VERSION}-${ARCH}-linux-gnu.tar.xz"
BASE_URL="https://github.com/espressif/qemu/releases/download/${QEMU_VERSION}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

echo "==> Downloading Espressif QEMU ${QEMU_VERSION} (${ARCH})"
curl --fail --location --retry 3 --progress-bar \
  -o "${TMP_DIR}/${ASSET}" "${BASE_URL}/${ASSET}"
curl --fail --location --retry 3 --silent \
  -o "${TMP_DIR}/checksum.sha256" \
  "${BASE_URL}/qemu-${QEMU_ASSET_VERSION}-checksum.sha256"

echo "==> Verifying SHA-256"
(
  cd "${TMP_DIR}"
  grep " \*\?${ASSET}\$" checksum.sha256 > "${ASSET}.sha256"
  sha256sum -c "${ASSET}.sha256"
)

echo "==> Installing to ${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}" "${BIN_DIR}"
tar -xJf "${TMP_DIR}/${ASSET}" -C "${INSTALL_DIR}" --strip-components=1
ln -sfn "${INSTALL_DIR}/bin/qemu-system-xtensa" "${BIN_DIR}/qemu-system-xtensa"

# The tarball keeps ROM images in share/qemu; QEMU finds them relative to the
# real executable path, so the symlink above is safe.
echo "==> Verifying esp32s3 machine"
"${BIN_DIR}/qemu-system-xtensa" --version | head -1
"${BIN_DIR}/qemu-system-xtensa" -machine help | grep -w esp32s3
echo "==> QEMU installed: ${BIN_DIR}/qemu-system-xtensa"
