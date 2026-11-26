#!/usr/bin/env bash
# Build the Emscripten preview into preview/tablo.{js,wasm,data}.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

LVGL_DIR="${LVGL_DIR:-${ROOT_DIR}/.pio/libdeps/esp32s3/lvgl}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/wasm}"
PREVIEW_DIR="${ROOT_DIR}/preview"

export PATH="${HOME}/.local/bin:${PATH}"

# The system Python is newer than what PlatformIO/Emscripten expect. Prepend the
# uv-managed Python 3.12 when it is installed.
UV_PYTHON_BIN="${UV_PYTHON_BIN:-${HOME}/.local/share/uv/python/cpython-3.12-linux-x86_64-gnu/bin}"
if [[ -d "${UV_PYTHON_BIN}" ]]; then
  export PATH="${UV_PYTHON_BIN}:${PATH}"
fi

if ! command -v emcc >/dev/null 2>&1; then
  EMSDK_ENV="${EMSDK_ENV:-${HOME}/.local/opt/emsdk/emsdk_env.sh}"
  if [[ ! -f "${EMSDK_ENV}" ]]; then
    echo "error: emcc is not available and ${EMSDK_ENV} was not found" >&2
    exit 1
  fi
  # shellcheck disable=SC1090
  export EMSDK_QUIET=1
  source "${EMSDK_ENV}" >/dev/null
fi

# emsdk_env.sh can reorder PATH; keep the uv interpreter in front.
if [[ -d "${UV_PYTHON_BIN}" ]]; then
  export PATH="${UV_PYTHON_BIN}:${PATH}"
fi

for tool in emcc emcmake cmake ninja; do
  if ! command -v "${tool}" >/dev/null 2>&1; then
    echo "error: ${tool} is not on PATH" >&2
    exit 1
  fi
done

if [[ ! -f "${LVGL_DIR}/CMakeLists.txt" ]]; then
  if ! command -v pio >/dev/null 2>&1; then
    echo "error: PlatformIO (pio) is not on PATH; cannot install LVGL 9.3" >&2
    exit 1
  fi
  echo "==> Installing PlatformIO dependencies (LVGL 9.3)…"
  (cd "${ROOT_DIR}" && pio pkg install -e esp32s3)
fi

if [[ ! -f "${LVGL_DIR}/CMakeLists.txt" ]]; then
  echo "error: LVGL was not found at ${LVGL_DIR}" >&2
  echo "       Run: pio pkg install -e esp32s3" >&2
  exit 1
fi

echo "==> Configuring WASM build"
emcmake cmake -S "${ROOT_DIR}/cmake/wasm" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLVGL_DIR="${LVGL_DIR}"

echo "==> Building tablo preview"
cmake --build "${BUILD_DIR}"

DIST_DIR="${BUILD_DIR}/dist"
for artifact in tablo.js tablo.wasm tablo.data; do
  if [[ ! -s "${DIST_DIR}/${artifact}" ]]; then
    echo "error: expected ${DIST_DIR}/${artifact} was not produced" >&2
    exit 1
  fi
done

# Publish only complete artifacts. The wasm is moved last: the browser polls its
# Last-Modified and must never see a partially written module.
mkdir -p "${PREVIEW_DIR}"
for artifact in tablo.js tablo.data tablo.wasm; do
  cp "${DIST_DIR}/${artifact}" "${PREVIEW_DIR}/.${artifact}.tmp"
  mv "${PREVIEW_DIR}/.${artifact}.tmp" "${PREVIEW_DIR}/${artifact}"
done

echo "==> Preview ready: ${PREVIEW_DIR}/index.html"
