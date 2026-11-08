#!/bin/sh
set -eu
cd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ -x port/build/native-setup-linux/Setup ]; then
    exec port/build/native-setup-linux/Setup "$@"
fi
exec python3 port/tools/native_setup.py "$@"
