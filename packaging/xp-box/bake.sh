#!/usr/bin/env bash
# Bake c-weather-gtk2/windows-xp-sp3 Vagrant box (no root — setup via ISO autorun).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CACHE="${XP_CACHE_DIR:-/tmp/c-weather-gtk2-xp-cache}"
VDI="$CACHE/xp.vdi"
ISO="$CACHE/setup.iso"
VM_NAME="c-weather-gtk2-xp-bake"
LOCAL_BOX="c-weather-gtk2/windows-xp-sp3"
BOX_FILE="$CACHE/windows-xp-sp3.box"

export VAGRANT_DOTFILE_PATH="$ROOT/.vagrant-xp-bake"

vm_state() {
  VBoxManage showvminfo "$VM_NAME" --machinereadable | grep '^VMState=' | cut -d= -f2 | tr -d '"'
}

trigger_setup() {
  echo "Triggering setup via keyboard (autorun disabled on XP)..."
  local vm="$VM_NAME"
  # Dismiss "Found New Hardware" wizard if present: Down x2, Tab, Enter.
  VBoxManage controlvm "$vm" keyboardputscancode 50 50 50 50 2>/dev/null || true
  sleep 0.3
  VBoxManage controlvm "$vm" keyboardputscancode 0f 8f 2>/dev/null || true
  sleep 0.3
  VBoxManage controlvm "$vm" keyboardputscancode 1c 9c 2>/dev/null || true
  sleep 0.5
  # Win+R, run startup.bat from CD.
  VBoxManage controlvm "$vm" keyboardputscancode e0 5b e0 db 2>/dev/null || true
  sleep 0.3
  VBoxManage controlvm "$vm" keyboardputscancode 13 93 2>/dev/null || true
  sleep 1
  VBoxManage controlvm "$vm" keyboardputstring "D:\\startup.bat" 2>/dev/null || true
  sleep 0.5
  VBoxManage controlvm "$vm" keyboardputscancode 1c 9c 2>/dev/null || true
}

# Wait until VM stays off for 90s (survives phase1 reboot).
wait_for_final_poweroff() {
  local timeout="${1:-3600}"
  local t=0
  local off_for=0
  local setup_triggered=0

  while (( t < timeout )); do
    local state
    state="$(vm_state)"

    if [[ "$state" == "poweroff" || "$state" == "aborted" ]]; then
      off_for=$((off_for + 15))
      echo "  VM state=$state off_for=${off_for}s (${t}s)"
      if (( off_for >= 90 )); then
        return 0
      fi
    else
      off_for=0
      echo "  VM state=$state (${t}s)"
      if [[ "$state" == "running" && "$setup_triggered" -eq 0 && "$t" -ge 120 ]]; then
        trigger_setup || true
        setup_triggered=1
      fi
    fi

    sleep 15
    t=$((t + 15))
  done

  echo "Timed out waiting for VM shutdown" >&2
  VBoxManage controlvm "$VM_NAME" poweroff 2>/dev/null || true
  return 1
}

create_vm() {
  if VBoxManage list vms | grep -q "\"$VM_NAME\""; then
    VBoxManage controlvm "$VM_NAME" poweroff 2>/dev/null || true
    sleep 3
    VBoxManage unregistervm "$VM_NAME" --delete 2>/dev/null || true
  fi

  VBoxManage createvm --name "$VM_NAME" --register --ostype WindowsXP
  VBoxManage modifyvm "$VM_NAME" \
    --memory 1024 \
    --vram 32 \
    --cpus 1 \
    --ioapic off \
    --pae on \
    --rtc-use-utc off \
    --audio-driver none \
    --usb off \
    --nic1 nat \
    --boot1 disk \
    --boot2 dvd

  VBoxManage storagectl "$VM_NAME" --name IDE --add ide --controller PIIX4
  VBoxManage storageattach "$VM_NAME" --storagectl IDE --port 0 --device 0 \
    --type hdd --medium "$VDI"
  VBoxManage storageattach "$VM_NAME" --storagectl IDE --port 1 --device 0 \
    --type dvddrive --medium "$ISO"
}

package_box() {
  echo "Packaging Vagrant box..."
  rm -f "$BOX_FILE"
  vagrant package --base "$VM_NAME" --output "$BOX_FILE" \
    --vagrantfile "$ROOT/packaging/xp-box/Vagrantfile.package"

  if vagrant box list | awk '{print $1}' | grep -qx "$LOCAL_BOX"; then
    vagrant box remove "$LOCAL_BOX" --force
  fi
  vagrant box add "$LOCAL_BOX" "$BOX_FILE" --force
  echo "Local box ready: $LOCAL_BOX"
}

main() {
  local resume=0
  if [[ "${1:-}" == "--resume" ]]; then
    resume=1
  fi

  "$ROOT/packaging/xp-box/download-assets.sh"
  "$ROOT/packaging/xp-box/build-setup-iso.sh"

  if (( resume )); then
    if ! VBoxManage list vms | grep -q "\"$VM_NAME\""; then
      echo "No bake VM to resume; run without --resume" >&2
      exit 1
    fi
    echo "Resuming setup on existing bake VM..."
    VBoxManage startvm "$VM_NAME" --type headless
  else
    create_vm
    echo "Starting XP VM (headless). Setup ISO installs GA + WinRM (~20–40 min)..."
    VBoxManage startvm "$VM_NAME" --type headless
  fi

  wait_for_final_poweroff 3600
  package_box
}

main "$@"
