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
# tag_link.c's own functions, without the service (harness unit)
if "$BIN" unit; then echo "PASS unit (P5: owed acknowledgements, same_addr)"; else echo "FAIL unit"; FAILED=1; fi
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
# calls <name> <env for the joiner> <polls after the fault at least> <early leaves>: a joiner playing PING (a match's
# inputs) while its service polls are answered as FAIL_POLLS says; its game must keep playing over UDP all along.
calls() {
    name=$1; joiner=$2; after=$3; early=$4
    "$BIN" host > "$TAGLINK_TEST_DIR/host.out" 2> "$TAGLINK_TEST_DIR/host.err" &
    HOST=$!
    for i in $(seq 50); do grep -q CODE "$TAGLINK_TEST_DIR/host.out" && break; sleep 0.2; done
    code=$(awk '/CODE/{print $2}' "$TAGLINK_TEST_DIR/host.out")
    out=$(env $joiner PING=20 timeout 120 "$BIN" join "$code" 2>&1)
    kill $HOST 2>/dev/null; wait $HOST 2>/dev/null
    line=$(printf '%s\n' "$out" | grep "^CALLS")
    a=$(printf '%s' "$line" | sed -n 's/.*after the first replaced one \([0-9]*\)).*/\1/p')
    e=$(printf '%s' "$line" | sed -n 's/.*before the game.s own end \([0-9]*\)).*/\1/p')
    echoes=$(printf '%s\n' "$out" | sed -n 's/.*PING \([0-9]*\) echoes.*/\1/p')
    if [ -n "$a" ] && [ "$a" -ge "$after" ] && [ "$e" = "$early" ] && [ "${echoes:-0}" -gt 500 ]; then echo "PASS $name ($line; $echoes echoes)"
    else echo "FAIL $name ($line; ${echoes:-0} echoes; wanted polls after >= $after, early leaves $early)"; FAILED=1; fi
}
# C1: a passing error of the service never takes a player out of the room; only "no room" does.
calls "C1 one 503 on a poll: polls go on, no leave" "FAIL_POLLS=2:503" 2 0
calls "C1 a Cloudflare 500 page on a poll (no JSON: polling goes on at its pace)" "FAIL_POLLS=2:500" 1 0
calls "C1 a poll without an answer (timeout)" "FAIL_POLLS=2:timeout" 2 0
calls "C1 503, 503, timeout in a row, then the service is back" "FAIL_POLLS=2:503,3:503,4:timeout" 2 0
calls "C1 no room: the player leaves and asks no more" "FAIL_POLLS=2:noroom" 0 1
calls "C1 a game that ends by itself says leave once, at its end" "COUNT_CALLS=1" 0 0
# background <name> <env for the host> <env for the joiner> <side whose calls are checked: host|join>: that side's app
# goes to the background (BG); its service is still asked about meanwhile (C2), and nothing leaves early.
background() {
    name=$1; hostenv=$2; joinenv=$3; side=$4
    env $hostenv "$BIN" host > "$TAGLINK_TEST_DIR/host.out" 2> "$TAGLINK_TEST_DIR/host.err" &
    HOST=$!
    for i in $(seq 50); do grep -q CODE "$TAGLINK_TEST_DIR/host.out" && break; sleep 0.2; done
    code=$(awk '/CODE/{print $2}' "$TAGLINK_TEST_DIR/host.out")
    out=$(env $joinenv COUNT_CALLS=1 PING=45 timeout 120 "$BIN" join "$code" 2>&1)
    wait $HOST 2>/dev/null # (the host ends by itself after 60 s, and counts its calls)
    if [ "$side" = host ]; then line=$(grep "^CALLS" "$TAGLINK_TEST_DIR/host.err"); else line=$(printf '%s\n' "$out" | grep "^CALLS"); fi
    b=$(printf '%s' "$line" | sed -n 's/.*in the background \([0-9]*\).*/\1/p')
    e=$(printf '%s' "$line" | sed -n 's/.*before the game.s own end \([0-9]*\)).*/\1/p')
    echoes=$(printf '%s\n' "$out" | sed -n 's/.*PING \([0-9]*\) echoes.*/\1/p')
    if [ "${b:-0}" -ge 2 ] && [ "$e" = 0 ] && [ "${echoes:-0}" -gt 1500 ]; then echo "PASS $name ($side: $line; $echoes echoes)"
    else echo "FAIL $name ($side: ${line:-no CALLS line}; ${echoes:-0} echoes; wanted 2+ polls in the background, no early leave)"; FAILED=1; fi
}
background "C2 the player's app in the background for 40 s" "" "BG=5000:40000" join
background "C2 the owner's app in the background for 40 s" "COUNT_CALLS=1 BG=5000:40000" "" host
# P3: the owner leaves a room while its thread asks the relay for an address (3 s) and opens another whose STUN takes 4 s
if env FAKE_RELAY=1 RELAY_ALLOC_MS=3000 STUN_MS=4000 "$BIN" restart 2> "$TAGLINK_TEST_DIR/restart.err"; then echo "PASS P3 $(grep RESTART "$TAGLINK_TEST_DIR/restart.err")"
else echo "FAIL P3 $(grep -E "RESTART|restart:" "$TAGLINK_TEST_DIR/restart.err")"; FAILED=1; fi
if env STUN_MS=1000 "$BIN" cycle 2> "$TAGLINK_TEST_DIR/cycle.err"; then echo "PASS P3 $(grep CYCLE "$TAGLINK_TEST_DIR/cycle.err")"
else echo "FAIL P3 $(grep CYCLE "$TAGLINK_TEST_DIR/cycle.err")"; FAILED=1; fi
[ $FAILED = 0 ] && echo "all passed"
exit $FAILED
