#!/usr/bin/env bash
set -euo pipefail

token_file="/run/secrets/shroomio-github-token"

mkdir -p /home/dev/.config/gh

if [ -f "$token_file" ] && [ -s "$token_file" ]; then
  if ! gh auth status >/dev/null 2>&1; then
    gh auth login --hostname github.com --with-token < "$token_file" >/dev/null
  fi
fi
