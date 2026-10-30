#!/bin/sh
# Builds and runs the automatic update's checks (test_update.c) with the system's C compiler and SDL3.
cd "$(dirname "$0")/../../.." || exit 1
O=port/build/test_update
mkdir -p port/build
gcc -std=gnu99 -O2 -Wall -o "$O" port/tests/update/test_update.c port/src/gs/update_sign.c port/src/gs/update_crypto.c \
    port/src/gs/update_monocypher.c -lSDL3 && "$O"
