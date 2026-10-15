#!/bin/sh
# Builds both release archives (Linux and Windows) in the container of port/release/Dockerfile:
#     port/release/build.sh   ->   port/build_release/Tenkaichi3Decomp-linux-x64.zip, Tenkaichi3Decomp-windows-x64.zip
# Needs only docker and this repository: no disc, no game data, no build on the host.
# BT3_BUILD_KEY in the environment (an official release's key: port/tools/buildkey.py, port/matchmaking/README.md) is
# handed on to the build by name, so its value is on no command line; without it the programs have no key.
set -e
cd "$(dirname "$0")/../.."
docker build -q -t bt3-port-release:jammy port/release >/dev/null
mkdir -p port/build_release
# the repository at /src with the release's own build folder in the place of port/build; files owned by you
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e BT3_BUILD_KEY -v "$PWD":/src -v "$PWD/port/build_release":/src/port/build -w /src \
    bt3-port-release:jammy sh port/release/inside.sh
echo "releases: port/build_release/"
