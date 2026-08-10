#!/usr/bin/env bash
# GTK2/CURL runtime DLLs that still run on Windows XP.
# Source: Inkscape Portable Legacy WinXP 0.92.3 (GTK+ 2, OpenSSL 1.0.x, no Vista APIs).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/toolchain/mingw32-xp"
STAMP="$DEST/.xp-runtime-stamp"
CACHE="${XP_RUNTIME_CACHE:-/tmp/c-weather-gtk2-xp-runtime}"
PAF_URL="${XP_INKSCAPE_PAF_URL:-https://sourceforge.net/projects/portableapps/files/Inkscape%20Portable/InkscapePortableLegacyWinXP_0.92.3.paf.exe/download}"
PAF="$CACHE/InkscapePortableLegacyWinXP_0.92.3.paf.exe"
EXTRACT="$CACHE/inkscape-xp"

if [[ -f "$STAMP" ]]; then
  echo "XP runtime already installed ($DEST)"
  exit 0
fi

for cmd in curl 7z; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
done

mkdir -p "$CACHE"

if [[ ! -f "$PAF" ]]; then
  echo "Downloading Inkscape Portable Legacy WinXP (~66 MiB)..."
  curl -fL --retry 3 -o "$PAF" "$PAF_URL"
fi

echo "Extracting XP GTK2/CURL runtime..."
rm -rf "$EXTRACT" "$DEST"
mkdir -p "$EXTRACT/App/Inkscape"
7z x -o"$EXTRACT" "$PAF" "App/Inkscape/*.dll" "App/Inkscape/lib/gdk-pixbuf-2.0/*" -y -bso0 -bse1 >/dev/null || true

INK="$EXTRACT/App/Inkscape"
if [[ ! -f "$INK/libglib-2.0-0.dll" ]]; then
  echo "Failed to extract Inkscape runtime from $PAF" >&2
  exit 1
fi

mkdir -p "$DEST/bin" "$DEST/lib/gdk-pixbuf-2.0"
cp -a "$INK"/*.dll "$DEST/bin/"
if [[ -d "$INK/lib/gdk-pixbuf-2.0" ]]; then
  cp -a "$INK/lib/gdk-pixbuf-2.0/." "$DEST/lib/gdk-pixbuf-2.0/"
fi

# Prefer modern CA bundle for HTTPS (XP OS store is unusable for today's CAs).
"$ROOT/packaging/ensure-ca-bundle.sh" "$DEST/etc/ssl/certs/ca-bundle.crt" >/dev/null

if [[ ! -f "$DEST/bin/libglib-2.0-0.dll" || ! -f "$DEST/bin/libgtk-win32-2.0-0.dll" ]]; then
  echo "XP runtime incomplete in $DEST" >&2
  exit 1
fi

date -Iseconds >"$STAMP"
echo "XP runtime ready: $DEST ($(find "$DEST/bin" -maxdepth 1 -name '*.dll' | wc -l) DLLs)"
