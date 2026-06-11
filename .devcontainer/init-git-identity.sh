#!/usr/bin/env bash
set -euo pipefail

name="${GIT_USER_NAME:-}"
email="${GIT_USER_EMAIL:-}"

if [ -f /home/dev/.gitconfig ]; then
  exit 0
fi

if [ -n "$name" ]; then
  git config --global user.name "$name"
fi

if [ -n "$email" ]; then
  git config --global user.email "$email"
fi
