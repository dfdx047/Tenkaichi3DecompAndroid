#!/bin/bash
# quick.sh <dev> <tag> [part...]: the 1.0 quick checks (2026-11-08) on one device (rp5 | cube | xiaomi), with the APK installed
# and the device's own settings (only meter=1 and the select are set). Parts (all when none is named):
#   bt3     the recorded COM vs COM fight (fight4.rec: menu, selection, fight, victory), the new select, state checksums
#           (BT3_HASH) and a picture every 10 s for the first 120 s (the selection is among them)
#   classic the same route with select_classic=1 for 100 s: pictures of the game's own selection
#   tag     the Tag Team's test fight, 5 against 5, COM vs COM (BT3_DEMO), 150 s
# Out: ~/DragonRageRefs/rel10/<tag>/. bt3_env.txt is removed at the end; settings restored from the copy taken at the start.
cd "$(dirname "$0")"; DEV=$1; T=$2; shift 2; PARTS=${*:-"bt3 classic tag"}
S=$(python3 -c "import lab; print(lab.DEVS['$DEV'])"); F=/sdcard/Android/data/com.dfdx047.dragonrage/files
O=$HOME/DragonRageRefs/rel10/$T; mkdir -p "$O"; export BENCH_OUT=$O
REC=$([ "$DEV" = rp5 ] && echo fight_rp5.rec || echo fight.rec)
adb -s $S push "$HOME/DragonRageRefs/fight4.rec" $F/$REC >/dev/null
adb -s $S shell cat $F/bt3_settings.txt > "$O/${DEV}_settings_before.txt"
shots() { for i in $(seq 1 $2); do sleep 10; adb -s $S exec-out screencap -p > "$O/${DEV}_$1_$(printf %03d $((i * 10))).png"; done; }
for P in $PARTS; do case $P in
  bt3)
    adb -s $S shell rm -f $F/hash.txt
    shots bt3 12 & python3 bench.py $DEV bt3 "meter=1 select_classic=0" "BT3_HASH=$F/hash.txt" 245; wait
    adb -s $S pull $F/hash.txt "$O/${DEV}_hash.txt" >/dev/null 2>&1 && adb -s $S shell rm -f $F/hash.txt
    echo "hash lines: $(wc -l < "$O/${DEV}_hash.txt" 2>/dev/null)"
    echo "crash: $(grep -a -c -iE 'SIGSEGV|SIGABRT|fatal signal' "$O/${DEV}_bt3.txt")" ;;
  classic)
    shots classic 10 & python3 bench.py $DEV classic "meter=1 select_classic=1" "" 100; wait
    echo "crash: $(grep -a -c -iE 'SIGSEGV|SIGABRT|fatal signal' "$O/${DEV}_classic.txt")" ;;
  tag)
    python3 -c "import lab; lab.stop('$S'); lab.settings_set('$S', ['meter=1'])"
    printf '%s\n' BT3_GS_VERBOSE=1 BT3_TAG_TEAM=1 BT3_TAG_ROSTER=5,5 BT3_DEMO=1 | adb -s $S shell "cat > $F/bt3_env.txt"
    sleep 1; adb -s $S shell am start -n com.dfdx047.dragonrage/.PlayShortcutActivity >/dev/null
    sleep 75; adb -s $S shell "top -H -b -n 1 -d 5 -p \$(pidof com.dfdx047.dragonrage:game)" > "$O/${DEV}_tag_threads.txt" 2>&1
    sleep 60; adb -s $S exec-out screencap -p > "$O/${DEV}_tag_end.png"
    python3 -c "import lab; lab.stop('$S')"; sleep 2
    adb -s $S pull $F/bt3_log.txt "$O/${DEV}_tag.txt" >/dev/null
    python3 bench.py summary "$O/${DEV}_tag.txt"
    grep -a -E "speed .*per frame" "$O/${DEV}_tag.txt" | tail -1 | cut -c1-200
    echo "crash: $(grep -a -c -iE 'SIGSEGV|SIGABRT|fatal signal' "$O/${DEV}_tag.txt")" ;;
esac; done
adb -s $S shell rm -f $F/bt3_env.txt
adb -s $S shell "cat > $F/bt3_settings.txt" < "$O/${DEV}_settings_before.txt"
echo QUICK-DONE
