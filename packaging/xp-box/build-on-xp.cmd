@echo off
setlocal
set "ROOT=V:\"
if exist C:\vagrant\Makefile.win32 set "ROOT=C:\vagrant\"
set "PATH=%ROOT%toolchain\mingw32\bin;%PATH%"
set "PKG_CONFIG=%ROOT%toolchain\mingw32\bin\pkg-config.exe"
set "PKG_CONFIG_PATH=%ROOT%toolchain\mingw32\lib\pkgconfig"
cd /d %ROOT%
set "MAKE=%ROOT%toolchain\mingw32\bin\mingw32-make.exe"
if not exist "%MAKE%" (
  echo Missing %MAKE%
  exit /b 1
)
"%ROOT%toolchain\mingw32\bin\gcc.exe" --version >nul 2>&1
if errorlevel 1 (
  echo gcc from toolchain does not run on this Windows version.
  echo Use an older MSYS2 i686 snapshot (GCC 8.x or earlier) or cross-build on Linux.
  exit /b 1
)
"%MAKE%" -f Makefile.win32 clean all STATIC=1 test CC=gcc
if errorlevel 1 exit /b 1
echo Native XP build OK
