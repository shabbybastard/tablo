#!/usr/bin/env python3
"""Print the CHANGELOG.md section for a version, for GitHub release notes."""
import re
import sys

if len(sys.argv) != 3:
    print("usage: release_notes.py VERSION CHANGELOG.md", file=sys.stderr)
    raise SystemExit(2)

version, path = sys.argv[1:3]
with open(path, encoding="utf-8") as fh:
    lines = fh.read().splitlines()

header = re.compile(r"^##\s+\[?v?" + re.escape(version) + r"(?:\]|\s|$)")
start = None
for index, line in enumerate(lines):
    if header.match(line):
        start = index + 1
        break

if start is None:
    print(f"tablo {version}")
    raise SystemExit(0)

end = len(lines)
for index in range(start, len(lines)):
    if lines[index].startswith("## "):
        end = index
        break

notes = "\n".join(lines[start:end]).strip()
print(notes or f"tablo {version}")
