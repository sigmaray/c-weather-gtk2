@echo off
REM Runs from setup ISO root (drive letter varies). Copy payload to C:\setup and start.
setlocal
set "SRC=%~dp0"
if not exist C:\setup mkdir C:\setup
xcopy /E /I /Y "%SRC%setup\*" C:\setup\ >nul
copy /Y "%~dp0setup\startup-launcher.bat" C:\setup\startup-launcher.bat >nul 2>nul
if not exist C:\setup\startup-launcher.bat (
  copy /Y "%SRC%..\packaging\xp-box\scripts\startup-launcher.bat" C:\setup\startup-launcher.bat >nul 2>nul
)

if not exist "C:\Documents and Settings\All Users\Start Menu\Programs\Startup" (
  mkdir "C:\Documents and Settings\All Users\Start Menu\Programs\Startup" 2>nul
)
copy /Y C:\setup\startup-launcher.bat "C:\Documents and Settings\All Users\Start Menu\Programs\Startup\c-weather-setup.bat" >nul

call C:\setup\startup-launcher.bat
