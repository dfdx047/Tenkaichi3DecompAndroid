#!/bin/sh
# A guest's inputs sent again (gs/tag_input_resend.h) against the owner's real kernel (shared/tag_kernel.c). C and C++17 compilers.
HERE=$(cd "$(dirname "$0")" && pwd)
INC="$HERE/../../include"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
cc -O1 -g -fsanitize=address,undefined -w -I"$INC" -c "$HERE/kernel_fixture.c" -o "$OUT/kernel_fixture.o" &&
cc -O1 -g -fsanitize=address,undefined -w -I"$INC" -c "$HERE/../../src/tag_team.c" -o "$OUT/tag_team.o" &&
cc -O1 -g -fsanitize=address,undefined -w -I"$INC" -c "$HERE/../../src/tag_team_wire.c" -o "$OUT/tag_team_wire.o" &&
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -w -I"$INC" -I"$HERE/../../src/gs" "$HERE/test.cpp" "$OUT/kernel_fixture.o" "$OUT/tag_team.o" "$OUT/tag_team_wire.o" -o "$OUT/test" || exit 1
"$OUT/test"
