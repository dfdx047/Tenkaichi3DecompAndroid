#!/bin/sh
# Shows the end of a log file as one annotation of the run (error by default, or the level given second), so the
# outcome of a build step can be read without access to the run's logs.
f="$1"; level="${2:-error}"
[ -f "$f" ] || exit 0
printf '::%s title=%s::' "$level" "$(basename "$f")"
tail -c 12000 "$f" | sed -e 's/%/%25/g' -e 's/\r//g' | awk '{printf "%s%%0A", $0}'
echo
