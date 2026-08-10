#!/usr/bin/env bash
# Download Windows XP VDI and WinRM/.NET installers for box baking.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CACHE="${XP_CACHE_DIR:-/tmp/c-weather-gtk2-xp-cache}"
mkdir -p "$CACHE"

VDI_ZIP="$CACHE/WnXPpro(sp3)_vdi.zip"
VDI="$CACHE/xp.vdi"
NETFX="$CACHE/NetFx20SP1_x86.exe"
WINRM="$CACHE/WindowsXP-KB968930-x86-ENG.exe"

download() {
  local url="$1"
  local dest="$2"
  if [[ -f "$dest" ]]; then
    echo "Already cached: $(basename "$dest")"
    return 0
  fi
  echo "Downloading $(basename "$dest")..."
  curl -fL --retry 3 --retry-delay 5 -o "$dest" "$url"
}

download \
  "https://raw.githubusercontent.com/hugsy/modern.ie-vagrant/master/installers/xp/NetFx20SP1_x86.exe" \
  "$NETFX"
download \
  "https://raw.githubusercontent.com/hugsy/modern.ie-vagrant/master/installers/xp/WindowsXP-KB968930-x86-ENG.exe" \
  "$WINRM"

if [[ ! -f "$VDI" ]]; then
  download "https://archive.org/download/xp51_20191108/WnXPpro(sp3)_vdi.zip" "$VDI_ZIP"
  echo "Extracting VDI (streaming, ~10 GiB)..."
  vdi_name="$(unzip -Z1 "$VDI_ZIP" '*.vdi' | head -1)"
  if [[ -z "$vdi_name" ]]; then
    echo "No .vdi inside $VDI_ZIP" >&2
    exit 1
  fi
  unzip -p "$VDI_ZIP" "$vdi_name" >"$VDI"
  rm -f "$VDI_ZIP"
else
  echo "Already cached: xp.vdi"
  rm -f "$VDI_ZIP"
fi

echo "Assets ready in $CACHE"
