#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if git ls-files --error-unmatch include/secrets.h >/dev/null 2>&1; then
  echo "SECURITY: include/secrets.h is tracked by git" >&2
  exit 1
fi

if [ -f include/secrets.h ]; then
  echo "include/secrets.h exists locally and is gitignored: OK"
else
  echo "include/secrets.h absent (using include/secrets.example.h): OK"
fi

if grep -RInE '(ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9]{20,})' \
    --exclude-dir=.git --exclude-dir=.pio --exclude-dir=build .; then
  echo "SECURITY: token-like string found" >&2
  exit 1
fi

echo "secrets check: OK"
