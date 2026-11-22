#!/bin/sh
# Builds the harness against port/src/gs/tag_link.c (the real one) and runs its regression cases against the local
# service (server.mjs, the real port/tagnet-worker). Needs Node 22 (node:sqlite), curl and a C compiler.
#   sh port/tools/taglink_test/run.sh          (from anywhere)
# A desktop SDL3 is used when SDL3_INCLUDE and SDL3_LIB are set; otherwise the POSIX stand-in in sdl3/ (test only).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
GS=$HERE/../../src/gs
export TAGLINK_TEST_DIR=${TAGLINK_TEST_DIR:-$(mktemp -d)}
BIN=$TAGLINK_TEST_DIR/harness
if [ -n "${SDL3_INCLUDE:-}" ]; then
    cc -O1 -w -I"$SDL3_INCLUDE" -I"$GS" "$HERE/harness.c" "$SDL3_LIB" -o "$BIN" || exit 1
else
    cc -O1 -w -I"$HERE/sdl3" -I"$GS" "$HERE/harness.c" -lpthread -o "$BIN" || exit 1
fi
node --no-warnings "$HERE/server.mjs" "$HERE/../../tagnet-worker" > "$TAGLINK_TEST_DIR/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null' EXIT INT TERM
for i in $(seq 50); do grep -q "server up" "$TAGLINK_TEST_DIR/server.log" 2>/dev/null && break; sleep 0.2; done
FAILED=0
# host_and_join <name> <env for both sides> <env for the joiner only> <what the joiner's output must contain>
host_and_join() {
    name=$1; both=$2; joiner=$3; want=$4
    env $both "$BIN" host > "$TAGLINK_TEST_DIR/host.out" 2> "$TAGLINK_TEST_DIR/host.err" &
    HOST=$!
    for i in $(seq 50); do grep -q CODE "$TAGLINK_TEST_DIR/host.out" && break; sleep 0.2; done
    code=$(awk '/CODE/{print $2}' "$TAGLINK_TEST_DIR/host.out")
    out=$(env $both $joiner timeout 120 "$BIN" join "$code" 2>&1)
    kill $HOST 2>/dev/null; wait $HOST 2>/dev/null
    if printf '%s\n' "$out" | grep -q -- "$want"; then echo "PASS $name"; else echo "FAIL $name"; printf '%s\n' "$out" | tail -5; FAILED=1; fi
}
host_and_join "stream echoed, no loss" "" "" "ALL BYTES MATCH"
host_and_join "stream echoed, 20% loss" "DROP=20" "" "ALL BYTES MATCH"
[ $FAILED = 0 ] && echo "all passed"
exit $FAILED
