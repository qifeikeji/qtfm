#!/bin/sh
# Build a drag-to-Applications DMG from a .app bundle (macOS only).
set -e
if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <path/to/App.app> <output.dmg> [volume-name]" >&2
  exit 1
fi
APP_SRC="$1"
OUT_DMG="$2"
VOLNAME="${3:-QtFM}"
if [ "$(uname -s)" != Darwin ]; then
  echo "make-macos-dmg.sh requires macOS" >&2
  exit 1
fi
if [ ! -d "$APP_SRC" ]; then
  echo "App bundle not found: $APP_SRC" >&2
  exit 1
fi
APP_SRC="$(cd "$(dirname "$APP_SRC")" && pwd)/$(basename "$APP_SRC")"
OUT_DMG="$(cd "$(dirname "$OUT_DMG")" 2>/dev/null && pwd)/$(basename "$OUT_DMG")" || OUT_DMG="$(pwd)/$(basename "$OUT_DMG")"

STAGING="$(mktemp -d /tmp/qtfm-dmg.XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT
rm -f "$OUT_DMG"

cp -R "$APP_SRC" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

hdiutil create -volname "$VOLNAME" -srcfolder "$STAGING" -ov -format UDZO "$OUT_DMG"
echo "Wrote $OUT_DMG"
