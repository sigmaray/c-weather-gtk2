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
    return 1
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
    return 1
  fi
  printf '%s\n' "$file"
}

# Skip heavy/unneeded runtime deps pulled by glib2/gtk2 etc.
skip_depend() {
  case "$1" in
    mingw-w64-i686-python|mingw-w64-i686-python-*|mingw-w64-i686-*-docs|\
    mingw-w64-i686-gobject-introspection*|mingw-w64-i686-*-icon-theme*|\
    mingw-w64-i686-adwaita-icon-theme*|mingw-w64-i686-hicolor-icon-theme*|\
    mingw-w64-i686-librsvg*|mingw-w64-i686-gtk-update-icon-cache*|\
    mingw-w64-i686-tzdata*)
      return 0 ;;
  esac
  return 1
}

# Strip version constraints: "mingw-w64-i686-atk>=1.29.2" -> "mingw-w64-i686-atk"
normalize_depend() {
  local dep="$1"
  dep="${dep%%[><=]*}"
  printf '%s\n' "$dep"
}

pkginfo_depends() {
  local archive="$1"
  # Prefer already-decompressed tar; else stream from zst.
  if [[ -f "${archive%.zst}.tar" ]]; then
    tar xf "${archive%.zst}.tar" -O .PKGINFO 2>/dev/null
  else
    tar -I zstd -xf "$archive" -O .PKGINFO 2>/dev/null
  fi | awk '/^depend = / { print $3 }'
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

# Index mirror once (package name -> latest archive).
echo "Indexing MinGW32 mirror..."
MAP_FILE="$CACHE/mingw32-pkg-index.txt"
LISTING="$CACHE/mingw32-listing.txt"
curl -fsSL "$MIRROR/" \
  | grep -oE 'mingw-w64-i686-[^"]+\.pkg\.tar\.zst' \
  | grep -vE '\-(docs|debug|dbgsym|static)-' \
  >"$LISTING"
python3 - "$MAP_FILE" "$LISTING" <<'PY'
import re, sys
from collections import defaultdict
out, listing = sys.argv[1], sys.argv[2]
buckets = defaultdict(list)
# Split at the version: last "-<digit>" before .pkg.tar.zst
pat = re.compile(r'^(mingw-w64-i686-.+?)-(\d[^/]*)\.pkg\.tar\.zst$')
with open(listing) as fh:
    for line in fh:
        line = line.strip()
        m = pat.match(line)
        if not m:
            continue
        buckets[m.group(1)].append(line)

lines = []
for name, files in buckets.items():
    files = sorted(set(files), key=lambda f: [int(x) if x.isdigit() else x for x in re.split(r'(\d+)', f)])
    lines.append(f"{name} {files[-1]}")
open(out, 'w').write('\n'.join(sorted(lines)) + '\n')
print(f"Indexed {len(lines)} packages", file=sys.stderr)
PY
lookup_pkg() {
  local name="$1"
  case "$name" in
    mingw-w64-i686-cc-libs) name=mingw-w64-i686-gcc-libs ;;
    mingw-w64-i686-cc) name=mingw-w64-i686-gcc ;;
  esac
  awk -v n="$name" '$1 == n { print $2; exit }' "$MAP_FILE"
}

echo "Fetching MinGW32 runtime/dev packages..."
SEEDS=(
  mingw-w64-i686-gcc-libs
  mingw-w64-i686-gtk2
  mingw-w64-i686-curl
  mingw-w64-i686-pkg-config
  mingw-w64-i686-cairo
  mingw-w64-i686-pango
  mingw-w64-i686-glib2
  mingw-w64-i686-gdk-pixbuf2
  mingw-w64-i686-libpng
  mingw-w64-i686-zlib
  mingw-w64-i686-openssl
  mingw-w64-i686-ca-certificates
  mingw-w64-i686-libwinpthread
  mingw-w64-i686-harfbuzz
  mingw-w64-i686-fontconfig
  mingw-w64-i686-freetype
  mingw-w64-i686-fribidi
  mingw-w64-i686-bzip2
  mingw-w64-i686-brotli
  mingw-w64-i686-expat
  mingw-w64-i686-pixman
  mingw-w64-i686-libjpeg-turbo
  mingw-w64-i686-libtiff
  mingw-w64-i686-libwebp
  mingw-w64-i686-libxml2
  mingw-w64-i686-libidn2
  mingw-w64-i686-atk
)

if [[ "${TOOLCHAIN_NATIVE_XP:-0}" == "1" ]]; then
  SEEDS+=(
    mingw-w64-i686-gcc
    mingw-w64-i686-binutils
    mingw-w64-i686-make
  )
fi

declare -A INSTALLED=()
QUEUE=("${SEEDS[@]}")

while ((${#QUEUE[@]})); do
  name="${QUEUE[0]}"
  QUEUE=("${QUEUE[@]:1}")
  [[ -n "${INSTALLED[$name]:-}" ]] && continue
  if skip_depend "$name"; then
    INSTALLED["$name"]=skip
    continue
  fi
  file="$(lookup_pkg "$name")"
  if [[ -z "$file" ]]; then
    # Fallback for odd version schemes (e.g. libwinpthread-git only).
    file="$(pick_latest "$name" 2>/dev/null || true)"
  fi
  if [[ -z "$file" ]]; then
    echo "WARNING: no archive for dependency $name" >&2
    INSTALLED["$name"]=missing
    continue
  fi
  fetch_pkg "$file"
  INSTALLED["$name"]="$file"
  while IFS= read -r dep; do
    [[ -z "$dep" ]] && continue
    dep="$(normalize_depend "$dep")"
    [[ -z "$dep" ]] && continue
    [[ -n "${INSTALLED[$dep]:-}" ]] && continue
    QUEUE+=("$dep")
  done < <(pkginfo_depends "$CACHE/$file")
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
