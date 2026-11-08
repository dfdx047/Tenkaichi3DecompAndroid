#!/bin/bash
# threads.sh <name>: once the Cube's BT4 is in a fight, the CPU of each thread (root): what takes the time on few cores
C=192.168.1.183:5555; B=/sdcard/Android/data/com.dfdx047.dragonrage/files/bt4; O=$HOME/DragonRageRefs/ab/logs/threads_$1.txt
for i in $(seq 1 120); do
  n=$(adb -s $C shell "grep -a -c 'logicrate' $B/bt3_log.txt" 2>/dev/null | tr -d '\r')
  [ "${n:-0}" -ge 3 ] && break; sleep 8
done
adb -s $C shell "su -c 'top -H -b -n 2 -d 15 -p \$(pidof com.dfdx047.dragonrage:bt4)'" > "$O" 2>&1
echo "$1 done: $(wc -l < "$O") lines"
