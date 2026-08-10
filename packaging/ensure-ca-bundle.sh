#!/usr/bin/env bash
# Ensure a Mozilla CA bundle exists; print its path on stdout.
# Usage: ensure-ca-bundle.sh [dest-file]
# If dest-file is given, copy/download there and print that path.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CACHE="${CA_BUNDLE_CACHE:-/tmp/c-weather-gtk2-ca-cache}"
DEST="${1:-}"

candidates=(
  "$ROOT/toolchain/mingw32/etc/ssl/certs/ca-bundle.crt"
  "$ROOT/toolchain/mingw32/ssl/certs/ca-bundle.crt"
  "$ROOT/toolchain/mingw32-xp/etc/ssl/certs/ca-bundle.crt"
  "$ROOT/packaging/curl-ca-bundle.crt"
  "/etc/ssl/certs/ca-certificates.crt"
)

src=""
for ca in "${candidates[@]}"; do
  if [[ -f "$ca" && -s "$ca" ]]; then
    src="$ca"
    break
  fi
done

if [[ -z "$src" ]]; then
  mkdir -p "$CACHE"
  cached="$CACHE/cacert.pem"
  if [[ ! -f "$cached" || ! -s "$cached" ]]; then
    echo "Downloading Mozilla CA bundle (cacert.pem)..." >&2
    curl -fsSL --retry 3 -o "$cached" "https://curl.se/ca/cacert.pem"
  fi
  src="$cached"
fi

if [[ -n "$DEST" ]]; then
  mkdir -p "$(dirname "$DEST")"
  cp -f "$src" "$DEST"
  echo "$DEST"
else
  echo "$src"
fi
