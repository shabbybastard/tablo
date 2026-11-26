#!/usr/bin/env python3
"""Create the ESP Web Tools manifest published on GitHub Pages.

The flasher writes the same app image that OTA downloads, plus the boot
images that a blank ESP32-S3 needs before the app at 0x10000.
"""
import json
import sys

if len(sys.argv) != 3:
    print("usage: make_flasher_manifest.py VERSION OUT.json", file=sys.stderr)
    raise SystemExit(2)

version, out_path = sys.argv[1:3]

manifest = {
    "name": "tablo",
    "version": version,
    "new_install_prompt_erase": True,
    "builds": [
        {
            "chipFamily": "ESP32-S3",
            "parts": [
                {"path": "bootloader.bin", "offset": 0},
                {"path": "partitions.bin", "offset": 32768},
                {"path": "boot_app0.bin", "offset": 57344},
                {"path": "firmware.bin", "offset": 65536},
            ],
        }
    ],
}

with open(out_path, "w", encoding="utf-8") as fh:
    json.dump(manifest, fh, ensure_ascii=False, indent=2)
    fh.write("\n")

print(json.dumps(manifest, ensure_ascii=False))
