#!/usr/bin/env bash
# Create an ISO with Guest Additions + WinRM setup for first XP boot.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CACHE="${XP_CACHE_DIR:-/tmp/c-weather-gtk2-xp-cache}"
SCRIPTS="$ROOT/packaging/xp-box/scripts"
ISO="$CACHE/setup.iso"
GA="$CACHE/vbox-ga/VBoxWindowsAdditions-x86.exe"

"$ROOT/packaging/xp-box/extract-ga.sh"

STAGE="$CACHE/setup-iso-root"
rm -rf "$STAGE"
mkdir -p "$STAGE/setup/vbox"

cp -f "$SCRIPTS/startup.bat" "$STAGE/startup.bat"
cp -f "$SCRIPTS/install-phase1.bat" "$STAGE/setup/"
cp -f "$SCRIPTS/install-phase2.bat" "$STAGE/setup/"
cp -f "$SCRIPTS/setup_winrm.bat" "$STAGE/setup/"
cp -f "$SCRIPTS/autologin.reg" "$STAGE/setup/"
cp -f "$SCRIPTS/winrm.reg" "$STAGE/setup/"
cp -f "$SCRIPTS/startup-launcher.bat" "$STAGE/setup/"
cp -f "$GA" "$STAGE/setup/vbox/"
cp -f "$CACHE/NetFx20SP1_x86.exe" "$STAGE/setup/"
cp -f "$CACHE/WindowsXP-KB968930-x86-ENG.exe" "$STAGE/setup/"

cat >"$STAGE/autorun.inf" <<'EOF'
[autorun]
open=startup.bat
label=C Weather GTK2 Setup
EOF

genisoimage -quiet -J -r -V "CWGTK2SETUP" -o "$ISO" "$STAGE"
echo "Setup ISO: $ISO"
