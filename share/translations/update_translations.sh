#!/usr/bin/env bash
# Regenerate qtfm_zh_CN.ts and .qm (requires python3 and Qt lrelease).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
python3 build_qtfm_ts.py
if command -v lrelease >/dev/null 2>&1; then
  lrelease qtfm_zh_CN.ts -qm qtfm_zh_CN.qm
  echo "Updated qtfm_zh_CN.qm"
else
  echo "lrelease not found; .ts updated only" >&2
fi
