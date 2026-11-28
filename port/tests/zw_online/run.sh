#!/bin/sh
# Z Warriors Online: the game's side on the PC, and the server's quick match under concurrent requests.
set -e
cd "$(dirname "$0")"
gcc -Wall -Wno-unused-function -g -fsanitize=address,undefined -o /tmp/zw_client_test client_test.c
/tmp/zw_client_test
node race.mjs
(cd ../../zw-worker && node test.mjs)
