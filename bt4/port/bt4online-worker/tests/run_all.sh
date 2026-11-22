#!/bin/bash
# The Worker's ranked validation (no device, no network): integrity, identity, end to end with spectators/reconnections/concurrency, and the rules guard. Exit 0 = all passed.
cd "$(dirname "$0")"; fail=0
for t in ranked_integrity ranked_identity ranked_e2e rules_guard; do
  out=$(node --no-warnings $t.test.mjs 2>&1); rc=$?
  echo "$t: $(echo "$out" | grep -c '^ok') ok, $(echo "$out" | grep -c '^FAIL') failed  -> $(echo "$out" | tail -1)"
  [ $rc = 0 ] || { fail=1; echo "$out" | grep '^FAIL'; }
done
[ $fail = 0 ] && echo "WORKER RANKED VALIDATION: ALL PASSED (simulated HTTP side only: no game, network or device)" || echo "WORKER RANKED VALIDATION: FAILED"; exit $fail
