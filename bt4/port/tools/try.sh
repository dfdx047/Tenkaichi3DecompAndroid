#!/bin/sh
# Development loop: copies the decompilation's uncommitted src/ and include/ changes into this tree, rebuilds,
# links and runs (where.sh arguments pass through). Before merging the committed decompilation again, discard the
# copies with `git checkout src include`.
cd "$(dirname "$0")/../.."
( cd ../bt3 && git diff --name-only -- src include ) | while read f; do cp "../bt3/$f" "$f"; done
python3 port/tools/undefined.py | grep -A3 FAILED
python3 port/tools/link.py > /dev/null
port/tools/where.sh "$@"
