#!/bin/bash
# dia_smoke.sh <tag> [secs]: the check after each change on the RP5: BT3 opens and fights (the recorded COM vs COM
# fight, bench.py), then Dragon Net connects (BT3_LOBBY=lobby: a lobby made through the service, its code in the log).
# The fight writes the state checksums (BT3_HASH) to hash.txt: compare_hash.py / the fight columns between builds.
# Logs and pictures in ~/DragonRageRefs/dia/<tag>/. bt3_env.txt is removed at the end (a left BT3_PAD_PLAY plays by itself).
cd "$(dirname "$0")"; T=$1; S=${2:-245}
R=192.168.1.89:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
export BENCH_OUT=$HOME/DragonRageRefs/dia/$T; mkdir -p "$BENCH_OUT"
adb -s $R push "$HOME/DragonRageRefs/fight4.rec" $F/fight_rp5.rec >/dev/null
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1 gpu_api=0 scale4=8 scale=2"
adb -s $R shell rm -f $F/hash.txt
python3 bench.py rp5 fight "$C" "BT3_HASH=$F/hash.txt" "$S"
adb -s $R pull $F/hash.txt "$BENCH_OUT/hash.txt" >/dev/null && adb -s $R shell rm -f $F/hash.txt
echo "hash lines: $(wc -l < "$BENCH_OUT/hash.txt")"
grep -a -c -E "SIGSEGV|SIGABRT|fatal signal|crash" "$BENCH_OUT/rp5_fight.txt" | sed 's/^/crash lines: /'
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
