#!/usr/bin/env bash
# Install Ubuntu mingw-w64 i686 cross-compiler without sudo (apt download + dpkg-deb -x).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/toolchain/cross-deb"
STAMP="$DEST/.cross-gcc-stamp"
CACHE="${CROSS_GCC_CACHE:-/tmp/c-weather-gtk2-cross-deb}"

if [[ -f "$STAMP" ]]; then
  echo "Cross compiler already unpacked ($DEST)"
  exit 0
fi

for cmd in apt-get dpkg-deb; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
done

PKGS=(
  gcc-mingw-w64-base
  gcc-mingw-w64-i686-posix-runtime
  binutils-mingw-w64-i686
  mingw-w64-common
  mingw-w64-i686-dev
  gcc-mingw-w64-i686-posix
  g++-mingw-w64-i686-posix
)

mkdir -p "$CACHE" "$DEST"
cd "$CACHE"
echo "Downloading cross-compiler packages..."
apt download "${PKGS[@]}"

rm -rf "$DEST"/*
for deb in ./*.deb; do
  dpkg-deb -x "$deb" "$DEST"
done

# Ubuntu names the driver *-posix; Makefile expects i686-w64-mingw32-gcc.
bindir="$DEST/usr/bin"
if [[ ! -x "$bindir/i686-w64-mingw32-gcc" && -x "$bindir/i686-w64-mingw32-gcc-posix" ]]; then
  ln -sf i686-w64-mingw32-gcc-posix "$bindir/i686-w64-mingw32-gcc"
  ln -sf i686-w64-mingw32-g++-posix "$bindir/i686-w64-mingw32-g++" 2>/dev/null || true
fi

# Ubuntu gcc resolves includes to $DEST/i686-w64-mingw32 and $DEST/share/mingw-w64.
if [[ -d "$DEST/usr/i686-w64-mingw32" && ! -e "$DEST/i686-w64-mingw32" ]]; then
  ln -s usr/i686-w64-mingw32 "$DEST/i686-w64-mingw32"
fi
if [[ -d "$DEST/usr/share/mingw-w64" && ! -e "$DEST/share" ]]; then
  mkdir -p "$DEST/share"
  ln -s ../usr/share/mingw-w64 "$DEST/share/mingw-w64"
fi

date -Iseconds >"$STAMP"
echo "Cross compiler ready: $bindir/i686-w64-mingw32-gcc"
