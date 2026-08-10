@echo off
if not exist C:\setup\phase1.done (
  call C:\setup\install-phase1.bat
  exit /b 0
)
if not exist C:\setup\phase2.done (
  call C:\setup\install-phase2.bat
)
