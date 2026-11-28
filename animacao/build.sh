#!/usr/bin/env bash
# Renders the 2D intro at 24 fps, synthesizes the sound and muxes both.
set -euo pipefail
cd "$(dirname "$0")"
node render.js 24
python3 audio.py audio.wav
ffmpeg -loglevel error -y -i dragon_rage_intro.mp4 -i audio.wav -map 0:v -map 1:a -c:v copy -c:a aac -b:a 256k -shortest dragon_rage_intro_som.mp4
rm -f audio.wav
mv dragon_rage_intro_som.mp4 dragon_rage_intro.mp4
echo "done: dragon_rage_intro.mp4"
