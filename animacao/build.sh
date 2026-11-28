#!/usr/bin/env bash
# Renders the 2D intro at 24 fps (supersampled 4K, downscaled to 1080p).
set -euo pipefail
cd "$(dirname "$0")"
node render.js 24
echo "done: dragon_rage_intro.mp4"
