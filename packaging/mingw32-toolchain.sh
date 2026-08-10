#!/usr/bin/env bash
# Fetch MinGW32 GTK2/libcurl libs from repo.msys2.org + Linux i686-w64-mingw32-gcc.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="$ROOT/toolchain/mingw32"
STAMP="$PREFIX/.toolchain-stamp"
MIRROR="https://repo.msys2.org/mingw/mingw32"
CACHE="${TOOLCHAIN_CACHE:-/tmp/c-weather-gtk2-toolchain-cache}"
CROSS_DEB="$ROOT/toolchain/cross-deb/usr/bin"

ensure_cross_compiler() {
  if command -v i686-w64-mingw32-gcc >/dev/null; then
    return 0
  fi
  if [[ -x "$CROSS_DEB/i686-w64-mingw32-gcc" ]]; then
    export PATH="$CROSS_DEB:$PATH"
    return 0
  fi
  "$ROOT/packaging/download-cross-gcc.sh"
  export PATH="$CROSS_DEB:$PATH"
}

if [[ -f "$STAMP" ]]; then
  ensure_cross_compiler
  echo "MinGW32 libs already installed ($PREFIX)"
  exit 0
fi

if [[ "${TOOLCHAIN_NATIVE_XP:-0}" != "1" ]]; then
  ensure_cross_compiler
else
  echo "Fetching MinGW32 libs for native XP build (no host cross-compiler)."
fi

for cmd in curl zstd tar python3; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
done

mkdir -p "$CACHE" "$ROOT/toolchain"

# Exact package name: next field must be a version (digit), not a suffix
# like curl-winssl / harfbuzz-utils / brotli-testdata / libwinpthread-git.
pick_latest() {
  local name="$1"
  local file
  file="$(curl -fsSL "$MIRROR/" \
    | grep -oE "${name}-[0-9][^\"]+\.pkg\.tar\.zst" \
    | grep -vE '\-(docs|debug|dbgsym|static)-' \
    | sort -uV \
    | tail -1)"
  if [[ -z "$file" ]]; then
    echo "No package matched: $name" >&2
    exit 1
  fi
  printf '%s\n' "$file"
}

pick_gcc() {
  local file
  file="$(curl -fsSL "$MIRROR/" \
    | grep -oE 'mingw-w64-i686-gcc-[0-9][^"]+\.pkg\.tar\.zst' \
    | sort -uV \
    | tail -1)"
  if [[ -z "$file" ]]; then
    echo "No package matched: mingw-w64-i686-gcc" >&2
    exit 1
  fi
  printf '%s\n' "$file"
}

fetch_pkg() {
  local file="$1"
  local url="$MIRROR/$file"
  if [[ -f "$CACHE/$file" ]]; then
    echo "  cached $file"
  else
    echo "  download $file"
    curl -fsSL -o "$CACHE/$file" "$url"
  fi
  local tar="$CACHE/${file%.zst}.tar"
  if [[ ! -f "$tar" ]]; then
    zstd -d -q -f -o "$tar" "$CACHE/$file"
  fi
  tar xf "$tar" -C "$ROOT/toolchain"
}

mkdir -p "$PREFIX"

echo "Fetching MinGW32 runtime/dev packages..."
SEEDS=(
  "$(pick_latest 'mingw-w64-i686-gcc-libs')"
  "$(pick_latest 'mingw-w64-i686-gtk2')"
  "$(pick_latest 'mingw-w64-i686-curl')"
  "$(pick_latest 'mingw-w64-i686-pkg-config')"
  "$(pick_latest 'mingw-w64-i686-cairo')"
  "$(pick_latest 'mingw-w64-i686-pango')"
  "$(pick_latest 'mingw-w64-i686-glib2')"
  "$(pick_latest 'mingw-w64-i686-gdk-pixbuf2')"
  "$(pick_latest 'mingw-w64-i686-libpng')"
  "$(pick_latest 'mingw-w64-i686-zlib')"
  "$(pick_latest 'mingw-w64-i686-openssl')"
  "$(pick_latest 'mingw-w64-i686-ca-certificates')"
  "$(pick_latest 'mingw-w64-i686-libwinpthread')"
  "$(pick_latest 'mingw-w64-i686-harfbuzz')"
  "$(pick_latest 'mingw-w64-i686-fontconfig')"
  "$(pick_latest 'mingw-w64-i686-freetype')"
  "$(pick_latest 'mingw-w64-i686-fribidi')"
  "$(pick_latest 'mingw-w64-i686-bzip2')"
  "$(pick_latest 'mingw-w64-i686-brotli')"
  "$(pick_latest 'mingw-w64-i686-expat')"
  "$(pick_latest 'mingw-w64-i686-pixman')"
  "$(pick_latest 'mingw-w64-i686-libjpeg-turbo')"
  "$(pick_latest 'mingw-w64-i686-libtiff')"
  "$(pick_latest 'mingw-w64-i686-libwebp')"
  "$(pick_latest 'mingw-w64-i686-libxml2')"
  "$(pick_latest 'mingw-w64-i686-libidn2')"
  "$(pick_latest 'mingw-w64-i686-atk')"
)

if [[ "${TOOLCHAIN_NATIVE_XP:-0}" == "1" ]]; then
  SEEDS+=(
    "$(pick_gcc)"
    "$(pick_latest 'mingw-w64-i686-binutils')"
    "$(pick_latest 'mingw-w64-i686-make')"
  )
fi

for pkg in "${SEEDS[@]}"; do
  [[ -n "$pkg" ]] || continue
  fetch_pkg "$pkg"
done

if [[ ! -f "$PREFIX/include/glib-2.0/glib.h" ]]; then
  echo "MinGW32 toolchain missing glib.h under $PREFIX" >&2
  exit 1
fi
if command -v pkg-config >/dev/null; then
  if ! PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR="$ROOT/toolchain" \
      pkg-config --exists gtk+-2.0 libcurl; then
    echo "pkg-config cannot resolve gtk+-2.0/libcurl after toolchain install" >&2
    PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR="$ROOT/toolchain" \
      pkg-config --errors-to-stdout --print-errors gtk+-2.0 libcurl >&2 || true
    exit 1
  fi
fi

date -Iseconds >"$STAMP"
ensure_cross_compiler
if command -v i686-w64-mingw32-gcc >/dev/null; then
  echo "Toolchain ready: $PREFIX (compiler: $(i686-w64-mingw32-gcc --version | head -1))"
else
  echo "Toolchain ready: $PREFIX (native XP build via V:\\toolchain\\mingw32\\bin\\gcc.exe)"
fi
