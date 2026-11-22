#!/bin/sh
# port/src/gs/tag_net.c's own code without the game (fast_test.c), built with AddressSanitizer. Needs a C compiler.
#   sh port/tools/tagnet_test/run.sh
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
cc -g -O1 -fsanitize=address -fno-omit-frame-pointer -w -I"$HERE/../taglink_test/sdl3" -I"$HERE/../../src/gs" -I"$HERE/../../src" \
   -I"$HERE/../../../include" "$HERE/fast_test.c" -lm -o "$OUT/fast_test" || exit 1
"$OUT/fast_test"
