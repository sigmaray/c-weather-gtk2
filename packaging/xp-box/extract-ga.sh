#!/usr/bin/env bash
# Extract VirtualBox Guest Additions x86 files for offline injection / setup.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CACHE="${XP_CACHE_DIR:-/tmp/c-weather-gtk2-xp-cache}"
OUT="$CACHE/vbox-ga"
ISO="${VBOX_GA_ISO:-/usr/share/virtualbox/VBoxGuestAdditions.iso}"

if [[ ! -f "$ISO" ]]; then
  echo "VBoxGuestAdditions.iso not found (set VBOX_GA_ISO)" >&2
  exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT"
7z x -o"$OUT" "$ISO" VBoxWindowsAdditions-x86.exe >/dev/null
echo "Guest Additions x86 extracted to $OUT"
