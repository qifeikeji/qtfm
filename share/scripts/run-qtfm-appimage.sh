#!/bin/sh
# Run QtFM AppImage when FUSE mount fails (common on some Arch/Manjaro setups).
set -e
APPIMAGE="${1:?Usage: run-qtfm-appimage.sh /path/to/qtfm-*.AppImage}"
shift
export APPIMAGE_EXTRACT_AND_RUN=1
exec "$APPIMAGE" "$@"
