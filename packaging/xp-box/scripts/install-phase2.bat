@echo off
setlocal
if exist C:\setup\phase2.done exit /b 0

echo [phase2] Installing .NET Framework 2.0 SP1...
C:\setup\NetFx20SP1_x86.exe /q /norestart
if errorlevel 1 echo .NET install warning (continuing)

echo [phase2] Installing WinRM (KB968930)...
C:\setup\WindowsXP-KB968930-x86-ENG.exe /q /norestart
if errorlevel 1 (
  echo WinRM install failed
  exit /b 1
)

regedit /s C:\setup\winrm.reg
call C:\setup\setup_winrm.bat
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v WinRM /t REG_SZ /d "C:\setup\setup_winrm.bat" /f

echo [phase2] Creating vagrant user...
net user vagrant vagrant /add
net localgroup administrators vagrant /add

echo done> C:\setup\phase2.done
echo [phase2] Shutting down for packaging...
shutdown /s /t 15 /f /c "XP box ready"
