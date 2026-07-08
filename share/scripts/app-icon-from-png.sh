#!/bin/sh
# Build share/images/QtFM.icns from canonical share/icons/mimes/app.png (macOS bundle / DMG).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CANON="$ROOT/share/icons/mimes/app.png"
PNG="$ROOT/share/images/app.png"
OUT="$ROOT/share/images/QtFM.icns"
if [ -f "$CANON" ]; then
  cp -f "$CANON" "$PNG"
fi
if [ ! -f "$PNG" ]; then
  echo "Missing $PNG (expected share/icons/mimes/app.png)" >&2
  exit 1
fi
if ! command -v iconutil >/dev/null 2>&1; then
  echo "iconutil not found (macOS only). Use app.png for Linux AppImage instead." >&2
  exit 1
fi
RESIZE=""
if command -v magick >/dev/null 2>&1; then
  RESIZE=magick
elif command -v convert >/dev/null 2>&1; then
  RESIZE=convert
fi
if [ -z "$RESIZE" ]; then
  echo "Need ImageMagick (magick or convert) to resize app.png" >&2
  exit 1
fi
ICONSET="$(mktemp -d /tmp/QtFM.iconset.XXXXXX)"
trap 'rm -rf "$ICONSET"' EXIT
for size in 16 32 128 256 512; do
  "$RESIZE" "$PNG" -resize "${size}x${size}" "$ICONSET/icon_${size}x${size}.png"
  double=$((size * 2))
  "$RESIZE" "$PNG" -resize "${double}x${double}" "$ICONSET/icon_${size}x${size}@2x.png"
done
iconutil -c icns "$ICONSET" -o "$OUT"
echo "Wrote $OUT"
