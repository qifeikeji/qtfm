#!/bin/sh
# Regenerate share/mimes.qrc after adding/removing files in share/icons/mimes/
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MIMES="$ROOT/share/icons/mimes"
OUT="$ROOT/share/mimes.qrc"
{
  echo '<RCC>'
  echo '    <qresource prefix="/">'
  for f in "$MIMES"/*.svg "$MIMES"/*.png; do
    [ -f "$f" ] || continue
    base=$(basename "$f")
    echo "        <file>icons/mimes/$base</file>"
  done
  echo '    </qresource>'
  echo '</RCC>'
} > "$OUT"
echo "Updated $OUT"
