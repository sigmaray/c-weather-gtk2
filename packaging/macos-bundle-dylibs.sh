#!/usr/bin/env bash
# Bundle non-system dylibs next to the binary and rewrite install names.
set -euo pipefail

bin="${1:?usage: $0 <binary> <dest-dir>}"
dest="${2:?usage: $0 <binary> <dest-dir>}"
name="$(basename "$bin")"

mkdir -p "$dest/lib"
cp -f "$bin" "$dest/$name"
chmod +x "$dest/$name"

collect_deps() {
  local target="$1"
  otool -L "$target" | awk 'NR>1 { print $1 }' | while read -r dep; do
    case "$dep" in
      /usr/lib/*|/System/*|@*) continue ;;
    esac
    if [[ -f "$dep" ]]; then
      echo "$dep"
    fi
  done
}

seen_file="$(mktemp)"
queue_file="$(mktemp)"
echo "$dest/$name" > "$queue_file"

while [[ -s "$queue_file" ]]; do
  cur="$(head -n1 "$queue_file")"
  tail -n +2 "$queue_file" > "${queue_file}.tmp" || true
  mv "${queue_file}.tmp" "$queue_file"

  while read -r dep; do
    base="$(basename "$dep")"
    if grep -Fxq "$base" "$seen_file" 2>/dev/null; then
      continue
    fi
    echo "$base" >> "$seen_file"
    cp -f "$dep" "$dest/lib/$base"
    install_name_tool -id "@loader_path/lib/$base" "$dest/lib/$base" 2>/dev/null || true
    echo "$dest/lib/$base" >> "$queue_file"
  done < <(collect_deps "$cur")
done

while read -r dep; do
  base="$(basename "$dep")"
  if [[ -f "$dest/lib/$base" ]]; then
    install_name_tool -change "$dep" "@loader_path/lib/$base" "$dest/$name"
  fi
done < <(collect_deps "$dest/$name")

for lib in "$dest"/lib/*; do
  [[ -e "$lib" ]] || continue
  while read -r dep; do
    base="$(basename "$dep")"
    if [[ -f "$dest/lib/$base" ]]; then
      install_name_tool -change "$dep" "@loader_path/$base" "$lib" 2>/dev/null || true
    fi
  done < <(collect_deps "$lib")
done

count="$(wc -l < "$seen_file" | tr -d ' ')"
rm -f "$seen_file" "$queue_file"
echo "Bundled $name with ${count} dylibs into $dest"
