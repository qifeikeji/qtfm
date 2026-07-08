#!/bin/sh
# Run QtFM AppImage when FUSE mount fails (common on some Arch/Manjaro setups).
set -e
APPIMAGE="${1:?Usage: run-qtfm-appimage.sh /path/to/qtfm-*.AppImage}"
shift

export APPIMAGE_EXTRACT_AND_RUN=1

# Host icon themes live under /usr/share/icons; AppImage AppRun often narrows XDG_DATA_DIRS.
case "${XDG_DATA_DIRS:-}" in
  */usr/share*) ;;
  *)
    if [ -n "${XDG_DATA_DIRS:-}" ]; then
      export XDG_DATA_DIRS="${XDG_DATA_DIRS}:/usr/share:/usr/local/share"
    else
      export XDG_DATA_DIRS="/usr/share:/usr/local/share"
    fi
    ;;
esac

# Avoid GTK platform theme overriding QIcon::setThemeName (same as Common::prepareLinuxIconThemeEnvironment).
if [ -z "${QT_QPA_PLATFORMTHEME:-}" ]; then
  export QT_QPA_PLATFORMTHEME=qtfm
fi

exec "$APPIMAGE" "$@"
