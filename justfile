# Windows XP Vagrant helpers for c-weather-gtk2.

set shell := ["bash", "-euo", "pipefail", "-c"]

local_box := "c-weather-gtk2/windows-xp-sp3"

doctor:
    #!/usr/bin/env bash
    set -euo pipefail
    if [[ -e /dev/vboxdrv ]] || [[ -e /dev/vboxdrvu ]]; then
      echo "VirtualBox OK: $(VBoxManage --version 2>/dev/null | head -1)"
      exit 0
    fi
    echo "VirtualBox provider not ready (/dev/vboxdrv missing)." >&2
    exit 1

# Download XP VDI + WinRM/.NET installers (~1.6 GiB VDI zip).
download:
    ./packaging/xp-box/download-assets.sh

# Bake and cache local Vagrant box (first run: download + ~30–60 min boot/provision).
box: doctor download
    #!/usr/bin/env bash
    set -euo pipefail
    if vagrant box list | awk '{print $1}' | grep -qx '{{local_box}}'; then
      echo "Local box already cached: {{local_box}}"
      exit 0
    fi
    chmod +x packaging/xp-box/*.sh
    ./packaging/xp-box/bake.sh

# Start Windows XP VM.
up: box
    vagrant up --provider=virtualbox

# Cross-compile i686 Windows binary on Linux (mingw32 toolchain, no sudo required).
build-win32:
    chmod +x packaging/*.sh packaging/xp-box/*.sh
    ./packaging/build-win32.sh clean all c-weather-gtk2-tests.exe STATIC=1

# Bundle portable dist for XP (exe + mingw32 DLLs; XP-compatible runtime when available).
bundle-win32: build-win32
    ./packaging/download-xp-runtime.sh
    ./packaging/windows-xp-bundle-dlls.sh ./c-weather-gtk2.exe dist-xp

# Run unit tests on XP via WinRM (headless).
test-xp: bundle-win32
    vagrant winrm -c "cmd /c V:\\dist-xp\\run-tests.cmd"

# Build inside XP VM (native i686 MinGW from shared toolchain/).
build-xp: up
    ./packaging/mingw32-toolchain.sh
    vagrant winrm -c "cd /d C:\\vagrant && packaging\\xp-box\\build-on-xp.cmd"

# Launch GUI app on XP (tray).
run-xp: bundle-win32
    vagrant winrm -c "cmd /c taskkill /IM c-weather-gtk2.exe /F" >/dev/null 2>&1 || true
    vagrant winrm -c "cmd /c V:\\dist-xp\\run-c-weather-gtk2.cmd"

halt:
    vagrant halt

destroy:
    vagrant destroy -f

# Remove baked box + VM artifacts.
clean-box: destroy
    #!/usr/bin/env bash
    set -euo pipefail
    vagrant box remove "{{local_box}}" --all --force 2>/dev/null || true
    rm -rf .vagrant .vagrant-xp-bake packaging/xp-box/cache/windows-xp-sp3.box
    VBoxManage controlvm c-weather-gtk2-xp-bake poweroff 2>/dev/null || true
    VBoxManage unregistervm c-weather-gtk2-xp-bake --delete 2>/dev/null || true
