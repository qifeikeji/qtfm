#!/bin/sh
# Single source of truth: share/icons/mimes/app.png → hicolor theme + share/images/app.png
# macOS .icns: run share/scripts/app-icon-from-png.sh on macOS after this script.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CANON="$ROOT/share/icons/mimes/app.png"
DEST="$ROOT/share/images/app.png"

if [ ! -f "$CANON" ]; then
  echo "Missing canonical app icon: $CANON" >&2
  exit 1
fi

RESIZE=""
if command -v magick >/dev/null 2>&1; then
  RESIZE=magick
elif command -v convert >/dev/null 2>&1; then
  RESIZE=convert
elif command -v ffmpeg >/dev/null 2>&1; then
  RESIZE=ffmpeg
elif command -v sips >/dev/null 2>&1 && [ "$(uname -s)" = Darwin ]; then
  RESIZE=sips
fi
if [ -z "$RESIZE" ]; then
  echo "Need magick, convert, ffmpeg, or macOS sips to resize app.png" >&2
  exit 1
fi

resize_png() {
  src="$1"
  size="$2"
  out="$3"
  mkdir -p "$(dirname "$out")"
  if [ "$RESIZE" = ffmpeg ]; then
    ffmpeg -y -loglevel error -i "$src" -vf "scale=${size}:${size}" "$out"
  elif [ "$RESIZE" = sips ]; then
    sips -z "$size" "$size" "$src" --out "$out" >/dev/null
  else
    "$RESIZE" "$src" -resize "${size}x${size}" "$out"
  fi
}

cp -f "$CANON" "$DEST"
echo "Synced $DEST from mimes/app.png"

for size in 16 20 22 24 32 36 48 64 72 96 128 160 192 256 512; do
  out="$ROOT/share/hicolor/${size}x${size}/apps/qtfm.png"
  resize_png "$CANON" "$size" "$out"
  echo "  hicolor/${size}x${size}/apps/qtfm.png"
done

echo "App icon sync done (canonical: share/icons/mimes/app.png)."
