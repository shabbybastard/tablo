"""Inject FW_VERSION into the ESP32 build.

The version comes from the repository `VERSION` file, which is the single
source for local builds and CI (`FW_VERSION` from the environment still wins if
set explicitly). The final fallback is `0.0.0-dev`. The value is passed as a
compiler definition so it is compiled into the firmware and compared with the
GitHub Pages manifest at runtime.
"""
import os

Import("env")  # noqa: F821

ROOT = env.subst("$PROJECT_DIR") or os.getcwd()
VERSION_FILE = os.path.join(ROOT, "VERSION")


def read_version_file():
    try:
        with open(VERSION_FILE, encoding="utf-8") as handle:
            return handle.read().strip()
    except OSError:
        return ""


version = os.environ.get("FW_VERSION", "").strip() or read_version_file() or "0.0.0-dev"
print(f"[tablo] FW_VERSION={version}")
env.Append(BUILD_FLAGS=[f'-DFW_VERSION=\\"{version}\\"'])
