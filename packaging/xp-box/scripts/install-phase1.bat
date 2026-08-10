@echo off
setlocal
if exist C:\setup\phase1.done exit /b 0

echo [phase1] Installing VirtualBox Guest Additions...
C:\setup\vbox\VBoxWindowsAdditions-x86.exe /S
if errorlevel 1 (
  echo Guest Additions install failed
  exit /b 1
)

echo [phase1] Enabling auto-login for Administrator...
regedit /s C:\setup\autologin.reg

echo done> C:\setup\phase1.done
echo [phase1] Rebooting...
shutdown /r /t 10 /f /c "Guest Additions installed"
