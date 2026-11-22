#!/bin/bash
# The offline validation suite of the select's compatibility (levels 1 and 2: no device, no game data). Exit 0 = all passed.
cd "$(dirname "$0")"; B=../../..; fail=0; T=$(mktemp -d)
for t in select_flow_test select_canon_test; do
  gcc -Wall -Werror -I$B/include -o $T/$t $t.c && $T/$t || fail=1
done
python3 static_audit.py $B || fail=1
# the changed units still compile for the host (a PC target): syntax only, the game is not built or run (no cmake, no SDL3, no game data here)
for f in src/menu/menu_c_e.c src/menu/menu_e_b.c src/battle/btl_seq.c; do
  if gcc -fsyntax-only -I$B/include -I$B/port/include -I$B/port/src -std=gnu89 -w -DPORT $B/$f 2>/dev/null; then echo "host syntax OK: $f"; else echo "host syntax FAILED: $f"; fail=1; fi
done
rm -rf $T; [ $fail = 0 ] && echo "SUITE: ALL PASSED (levels 1-2 only: this proves nothing about a real match)" || echo "SUITE: FAILED"; exit $fail
