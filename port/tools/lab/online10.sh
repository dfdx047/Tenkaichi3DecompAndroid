#!/bin/bash
# online10.sh <ver> [secs]: direct session Cube <-> RP5 with random input (BT3_NET_SCRIPT), both sides' logs in <ver>/logs. The Cube's networking restriction is lifted and put back.
cd "$(dirname "$0")"; V=$1; T=${2:-600}; O=$HOME/DragonRageRefs/$V/logs; mkdir -p "$O"
C=192.168.1.183:5555; R=192.168.1.89:5555; RIP=192.168.1.89; F=/sdcard/Android/data/com.dfdx047.dragonrage/files; PORT=7777
adb -s $C shell settings put global restricted_networking_mode 0
for s in $C $R; do
  adb -s $s shell am force-stop com.dfdx047.dragonrage
  adb -s $s push ../net_rand.tab $F/net_rand.tab >/dev/null 2>&1
  adb -s $s shell rm -f $F/bt3_log.txt $F/bt3_log.1.txt $F/bt3_log.2.txt
done
adb -s $R shell "printf 'BT3_NET_HOST=$PORT\nBT3_NET_SESSION=1\nBT3_NET_SCRIPT=$F/net_rand.tab\nBT3_GS_VERBOSE=1\n${HOST_ENV:+$HOST_ENV\n}' > $F/bt3_env.txt"
adb -s $C shell "printf 'BT3_NET_JOIN=$RIP:$PORT\nBT3_NET_SESSION=1\nBT3_NET_SCRIPT=$F/net_rand.tab\nBT3_GS_VERBOSE=1\n' > $F/bt3_env.txt"
python3 lab.py rp5 start; sleep 2; python3 lab.py cube start
# The session can end by itself (a button that leaves the mode): stop waiting and say so, not a run that looks complete
E=0; while [ $E -lt "$T" ]; do sleep 20; E=$((E+20))
  for s in $C $R; do
    if adb -s $s shell "grep -a -q 'the session has ended' $F/bt3_log.txt"; then echo "INVALID: the session ended after about $E s (on $s)"; E=$T; break; fi
  done
  echo "t=$E"
done
adb -s $C exec-out screencap -p > "$O/online_cube.png"; adb -s $R exec-out screencap -p > "$O/online_rp5.png"
for p in cube:$C rp5:$R; do n=${p%%:*}; s=${p#*:}
  adb -s $s shell "cat $F/bt3_log.2.txt $F/bt3_log.1.txt $F/bt3_log.txt 2>/dev/null" > "$O/online_$n.txt"
  adb -s $s shell am force-stop com.dfdx047.dragonrage
  adb -s $s shell rm -f $F/bt3_env.txt $F/net_rand.tab
done
adb -s $C shell settings put global restricted_networking_mode 1
echo "BT3_GS_VERBOSE=1" | adb -s $C shell "cat > $F/bt3_env.txt"
for n in cube rp5; do L="$O/online_$n.txt"
  echo "== $n: $(grep -a -c 'the two games differ' "$L") differ; lines $(wc -l < "$L")"
  grep -a -E 'bt3: net: (hosting|joining|connected|the two games|comparisons|session|the other)' "$L" | head -6 | cut -c1-170
  grep -a -E 'bt3: alive' "$L" | tail -1 | cut -c1-110
  grep -a -E 'bt3: net: packets out' "$L" | tail -1 | cut -c1-170
  grep -a -c -E 'SIGSEGV|fatal|crash' "$L"
done
