#!/bin/bash
# dia_lobby.sh <tag>: only the Dragon Net part of dia_smoke.sh (a lobby made through the service, its code in the log).
cd "$(dirname "$0")"; T=$1
R=192.168.1.89:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
export BENCH_OUT=$HOME/DragonRageRefs/dia/$T; mkdir -p "$BENCH_OUT"
# Dragon Net: a lobby by code from the service
adb -s $R shell am force-stop com.dfdx047.dragonrage
printf 'BT3_GS_VERBOSE=1\nBT3_LOBBY=lobby\nBT3_LOBBY_AT=300\n' | adb -s $R shell "cat > $F/bt3_env.txt"
sleep 1; adb -s $R shell am start -n com.dfdx047.dragonrage/.PlayShortcutActivity >/dev/null
sleep 60
adb -s $R exec-out screencap -p > "$BENCH_OUT/rp5_lobby.png"
adb -s $R shell am force-stop com.dfdx047.dragonrage; sleep 2
adb -s $R pull $F/bt3_log.txt "$BENCH_OUT/rp5_lobby.txt" >/dev/null
adb -s $R shell rm -f $F/bt3_env.txt
echo "dragon net: $(grep -a -E "(bt3: lobby: |\[lobby\] )(code|made)" "$BENCH_OUT/rp5_lobby.txt" | head -2 | tr '\n' ' ')"
grep -a -E "bt3: (lobby|net).*(refus|fail|could not|no answer|error)" "$BENCH_OUT/rp5_lobby.txt" | head -3
echo SMOKE-DONE
