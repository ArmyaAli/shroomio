#!/usr/bin/env bash
set -euo pipefail

copy_tree() {
  local src="$1"
  local dst="$2"

  mkdir -p "$dst"
  if [ -d "$src" ]; then
    chmod -R u+rwX "$dst" 2>/dev/null || true
    find "$dst" -mindepth 1 -delete
    cp -a "$src"/. "$dst"/
    chmod -R u+rwX "$dst" 2>/dev/null || true
  fi
}

copy_tree /mnt/host-opencode/config /home/dev/.config/opencode
copy_tree /mnt/host-opencode/share /home/dev/.local/share/opencode
copy_tree /mnt/host-opencode/state /home/dev/.local/state/opencode
copy_tree /mnt/host-opencode/cache /home/dev/.cache/opencode
