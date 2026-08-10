#!/usr/bin/env bash
# Wrapper: ensure cross compiler + MinGW32 libs, then build win32 binaries.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/toolchain/cross-deb/usr/bin:$PATH"
"$ROOT/packaging/mingw32-toolchain.sh"
export CC=i686-w64-mingw32-gcc
export CXX=i686-w64-mingw32-g++
exec make -f "$ROOT/Makefile.win32" "$@"
