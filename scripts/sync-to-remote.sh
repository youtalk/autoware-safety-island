#!/usr/bin/env bash
# Rsync the working tree to the development host.
# Usage: ./scripts/sync-to-remote.sh
set -euo pipefail
REMOTE=${REMOTE:-test@100.119.163.33}
REMOTE_PATH=${REMOTE_PATH:-/home/test/youtalk/autoware-safety-island}
rsync -av --delete \
    --exclude '.git' \
    --exclude 'build*' \
    --exclude 'build-s32z2*' \
    --exclude 'build-freertos*' \
    --exclude '*.pyc' \
    --exclude '__pycache__' \
    "$(git rev-parse --show-toplevel)/" \
    "${REMOTE}:${REMOTE_PATH}/"
