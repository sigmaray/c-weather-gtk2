#!/usr/bin/env bash
# Copy MinGW DLLs required by a PE binary into DEST dir (portable Windows package).
set -euo pipefail

bin="${1:?usage: $0 <exe> <dest-dir>}"
dest="${2:?usage: $0 <exe> <dest-dir>}"

mkdir -p "$dest"
cp -f "$bin" "$dest/"

mingw_root="$(cygpath -u "${MINGW_PREFIX:-/mingw64}" 2>/dev/null || echo /mingw64)"

# Transitive closure of MinGW DLLs (ldd on exe alone misses deps of deps).
declare -A copied=()
copy_mingw_dll() {
  local path="$1"
  [[ -n "$path" && -f "$path" ]] || return 0
  local base
  base="$(basename "$path")"
  [[ -n "${copied[$base]:-}" ]] && return 0
  cp -f "$path" "$dest/"
  copied["$base"]=1
  local dep
  while IFS= read -r dep; do
    copy_mingw_dll "$dep"
  done < <(ldd "$path" 2>/dev/null | awk '/\/mingw64\/bin\// { print $3 }' | sort -u)
}

copy_mingw_dll "$bin"

# Server Core / headless Windows have no system opengl32.dll; GTK/libepoxy need OpenGL.
# Mesa ships opengl32.dll + libgallium_wgl.dll (loaded at runtime via LoadLibrary).
mesa_opengl="$mingw_root/bin/opengl32.dll"
if [[ -f "$mesa_opengl" ]]; then
  copy_mingw_dll "$mesa_opengl"
fi

# GDK pixbuf loaders (loaded at runtime, not always in ldd).
loader_dir="$mingw_root/lib/gdk-pixbuf-2.0"
if [[ -d "$loader_dir" ]]; then
  mkdir -p "$dest/lib/gdk-pixbuf-2.0"
  cp -a "$loader_dir/." "$dest/lib/gdk-pixbuf-2.0/"
fi

# CA bundle for OpenSSL-backed libcurl (portable zip has no system certs).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/packaging/ensure-ca-bundle.sh" "$dest/curl-ca-bundle.crt" >/dev/null
if [[ ! -s "$dest/curl-ca-bundle.crt" ]]; then
  echo "Failed to install curl-ca-bundle.crt into $dest" >&2
  exit 1
fi

# Launcher for RDP/Explorer++ (sets GTK pixbuf path; exe dir first in PATH).
cat > "$dest/run-c-weather-gtk2.cmd" <<'EOF'
@echo off
cd /d "%~dp0"
set "PATH=%~dp0;%PATH%"
set "CURL_CA_BUNDLE=%~dp0curl-ca-bundle.crt"
set "SSL_CERT_FILE=%~dp0curl-ca-bundle.crt"
set "GDK_PIXBUF_MODULE_FILE=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders.cache"
start "" "%~dp0c-weather-gtk2.exe"
EOF

echo "Packaged $(basename "$bin") + ${#copied[@]} DLLs into $dest"
