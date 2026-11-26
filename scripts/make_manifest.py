#!/usr/bin/env python3
"""Create the OTA manifest published on GitHub Pages."""
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

if len(sys.argv) != 4:
    print("usage: make_manifest.py VERSION FIRMWARE.bin OUT.json", file=sys.stderr)
    raise SystemExit(2)

version, firmware, out_path = sys.argv[1:4]
if not os.path.isfile(firmware):
    print(f"firmware not found: {firmware}", file=sys.stderr)
    raise SystemExit(1)

with open(firmware, "rb") as fh:
    digest = hashlib.sha256(fh.read()).hexdigest()

manifest = {
    "version": version,
    "sha256": digest,
    "bin_url": "https://shabbybastard.github.io/tablo/firmware.bin",
    "size": os.path.getsize(firmware),
    "released_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
}

with open(out_path, "w", encoding="utf-8") as fh:
    json.dump(manifest, fh, ensure_ascii=False, indent=2)
    fh.write("\n")

print(json.dumps(manifest, ensure_ascii=False))
