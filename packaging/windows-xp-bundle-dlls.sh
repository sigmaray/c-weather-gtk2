#!/usr/bin/env bash
# Bundle i686 MinGW DLLs for portable Windows XP package.
set -euo pipefail

bin="${1:?usage: $0 <exe> <dest-dir>}"
dest="${2:?usage: $0 <exe> <dest-dir>}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mingw_root="${MINGW32_PREFIX:-$ROOT/toolchain/mingw32}"
if [[ -d "$ROOT/toolchain/mingw32-xp/bin" ]]; then
  mingw_root="$ROOT/toolchain/mingw32-xp"
fi
objdump="${OBJDUMP:-}"
if [[ -z "$objdump" ]]; then
  if command -v i686-w64-mingw32-objdump >/dev/null; then
    objdump=i686-w64-mingw32-objdump
  elif [[ -x "$ROOT/toolchain/cross-deb/usr/bin/i686-w64-mingw32-objdump" ]]; then
    objdump="$ROOT/toolchain/cross-deb/usr/bin/i686-w64-mingw32-objdump"
  else
    objdump=objdump
  fi
fi

mkdir -p "$dest"
find "$dest" -mindepth 1 -maxdepth 1 ! -name 'run-*.cmd' -exec rm -rf {} +
cp -f "$bin" "$dest/"

declare -A copied=()

find_mingw_dll() {
  local name="$1"
  case "${name,,}" in
    kernel32.dll|msvcrt.dll|user32.dll|gdi32.dll|advapi32.dll|shell32.dll|ole32.dll|ws2_32.dll|winmm.dll|imm32.dll|comctl32.dll|comdlg32.dll|crypt32.dll|wldap32.dll|msimg32.dll|uuid.dll|rpcrt4.dll|ntdll.dll)
      return 0 ;;
  esac
  if [[ -f "$mingw_root/bin/$name" ]]; then
    echo "$mingw_root/bin/$name"
    return 0
  fi
  local lower="${name,,}" f base
  for f in "$mingw_root/bin"/*; do
    base="${f##*/}"
    if [[ "${base,,}" == "$lower" ]]; then
      echo "$f"
      return 0
    fi
  done
  return 0
}

copy_mingw_dll() {
  local path="$1"
  [[ -n "$path" && -f "$path" ]] || return 0
  local base
  base="$(basename "$path")"
  [[ -n "${copied[$base]:-}" ]] && return 0
  cp -f "$path" "$dest/"
  copied["$base"]=1
  local dep name
  while IFS= read -r name; do
    dep="$(find_mingw_dll "$name")"
    [[ -n "$dep" ]] && copy_mingw_dll "$dep"
  done < <($objdump -p "$path" 2>/dev/null | awk '/DLL Name:/ {print $3}') || true
}

copy_mingw_dll "$bin"

loader_dir="$mingw_root/lib/gdk-pixbuf-2.0"
if [[ -d "$loader_dir" ]]; then
  mkdir -p "$dest/lib/gdk-pixbuf-2.0"
  cp -a "$loader_dir/." "$dest/lib/gdk-pixbuf-2.0/"
fi

# Portable OpenSSL/libcurl needs an explicit CA file; XP store is too old.
"$ROOT/packaging/ensure-ca-bundle.sh" "$dest/curl-ca-bundle.crt" >/dev/null
if [[ ! -s "$dest/curl-ca-bundle.crt" ]]; then
  echo "Failed to install curl-ca-bundle.crt into $dest" >&2
  exit 1
fi

cat > "$dest/run-c-weather-gtk2.cmd" <<'EOF'
@echo off
cd /d "%~dp0"
set "PATH=%~dp0;%PATH%"
set "CURL_CA_BUNDLE=%~dp0curl-ca-bundle.crt"
set "SSL_CERT_FILE=%~dp0curl-ca-bundle.crt"
set "GDK_PIXBUF_MODULE_FILE=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders.cache"
start "" "%~dp0c-weather-gtk2.exe"
EOF

cat > "$dest/run-tests.cmd" <<'EOF'
@echo off
cd /d "%~dp0"
set "PATH=%~dp0;%PATH%"
set "CURL_CA_BUNDLE=%~dp0curl-ca-bundle.crt"
set "SSL_CERT_FILE=%~dp0curl-ca-bundle.crt"
c-weather-gtk2-tests.exe
exit /b %ERRORLEVEL%
EOF

cp -f "$ROOT/c-weather-gtk2-tests.exe" "$dest/" 2>/dev/null || true

echo "Packaged $(basename "$bin") + ${#copied[@]} DLLs into $dest"
