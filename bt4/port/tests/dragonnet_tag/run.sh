#!/bin/sh
# gs/dragonnet_tag.cpp's retries, without the network (test.cpp). Needs a C++17 compiler.
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -w -I"$HERE/sdl3" -I"$HERE/../../src/gs" "$HERE/test.cpp" -lpthread -o "$OUT/test" || exit 1
"$OUT/test"
