#!/usr/bin/env bash
set -euo pipefail

# User-local toolchain for tablo. No sudo, nothing outside ~/.local.
export PATH="$HOME/.local/bin:$PATH"
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v uv >/dev/null 2>&1; then
  curl -LsSf https://astral.sh/uv/install.sh | sh
  export PATH="$HOME/.local/bin:$PATH"
fi
uv python install 3.12
if ! command -v pio >/dev/null 2>&1; then
  uv tool install --python 3.12 'platformio==6.2.0'
fi

if [ ! -d "$HOME/.local/opt/emsdk/.git" ]; then
  mkdir -p "$HOME/.local/opt"
  git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$HOME/.local/opt/emsdk"
fi
(
  cd "$HOME/.local/opt/emsdk"
  ./emsdk install 4.0.23
  ./emsdk activate 4.0.23
)

if ! command -v node >/dev/null 2>&1; then
  V=v26.13.1
  mkdir -p "$HOME/.local/opt"
  tmp="$(mktemp -d)"
  curl -sSL -o "$tmp/node.tar.xz" "https://nodejs.org/dist/$V/node-$V-linux-x64.tar.xz"
  tar -xf "$tmp/node.tar.xz" -C "$tmp"
  rm -rf "$HOME/.local/opt/node"
  mv "$tmp/node-$V-linux-x64" "$HOME/.local/opt/node"
  rm -rf "$tmp"
  ln -sf "$HOME/.local/opt/node/bin/node" "$HOME/.local/bin/node"
  ln -sf "$HOME/.local/opt/node/bin/npx" "$HOME/.local/bin/npx"
  ln -sf "$HOME/.local/opt/node/bin/npm" "$HOME/.local/bin/npm"
fi

MARK_BEGIN="# >>> tablo toolchain >>>"
MARK_END="# <<< tablo toolchain <<<"
for rcfile in "$HOME/.bashrc" "$HOME/.profile"; do
  if [ -f "$rcfile" ] && ! grep -qF "$MARK_BEGIN" "$rcfile"; then
    cp "$rcfile" "$rcfile.tablo.bak" 2>/dev/null || true
    {
      echo ""
      echo "$MARK_BEGIN"
      echo 'export PATH="$HOME/.local/bin:$HOME/.local/opt/emsdk/upstream/emscripten:$HOME/.local/opt/emsdk:$PATH"'
      echo "$MARK_END"
    } >> "$rcfile"
  fi
done

# shellcheck disable=SC1091
source "$HOME/.local/opt/emsdk/emsdk_env.sh" >/dev/null 2>&1

if ! command -v qemu-system-xtensa >/dev/null 2>&1 ||
   ! qemu-system-xtensa -machine help 2>/dev/null | grep -qw esp32s3; then
  "${ROOT_DIR}/scripts/install_qemu.sh"
fi

echo "PlatformIO: $(pio --version)"
echo "Emscripten: $(emcc --version | head -1)"
echo "Node: $(node --version)"
echo "QEMU: $(qemu-system-xtensa --version | head -1)"
