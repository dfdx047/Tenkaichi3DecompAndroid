#!/bin/sh
# Puts Budokai Tenkaichi 4's engine (BT4decomp) into the app before Gradle runs: libbt4dec.so and libdatachannel.so
# (online TAG TEAM) in android/app/src/main/jniLibs/arm64-v8a, its files in android/app/src/main/assets/bt4dec, and
# their licences at the end of assets/licenses.txt.
#
# The engine is not built here: it comes from the branch bt4dec-proto (NauTaro's decompilation of the BT4 mod with
# Dragon Rage's Android port, port/tools/android.py with BT3_SKELETON=1, so no game data is in it) and
# libdatachannel from its pinned sources (rtc-sources.lock.json in the bundle), both built locally and attached to
# the draft release "bt4dec-engine" as bt4dec-engine-android-arm64.tar.xz. A new engine: build it, pack it the same way
# and replace that file in that draft (on its page: Edit, delete the old file, attach the new one).
#
#   sh .github/bt4dec-engine.sh            (GH_TOKEN set: the Actions token is enough)
set -eu
TAG=bt4dec-engine
FILE=bt4dec-engine-android-arm64.tar.xz
APP=android/app/src/main
TMP=$(mktemp -d)
REPO=${GITHUB_REPOSITORY:-dfdx047/Tenkaichi3DecompAndroid}
# (the release is a draft, seen only by the repository's collaborators and its Actions token: found by its name in
# the list, as a draft has no tag yet, and its file fetched through the API)
ASSET=$(gh api "repos/$REPO/releases?per_page=100" --jq "[.[] | select(.name == \"$TAG\" or .tag_name == \"$TAG\") | .assets[] | select(.name == \"$FILE\") | .id][0]")
[ -n "$ASSET" ] && [ "$ASSET" != "null" ] || { echo "::error::no $FILE in a release named $TAG"; exit 1; }
gh api -H "Accept: application/octet-stream" "repos/$REPO/releases/assets/$ASSET" > "$TMP/$FILE"
mkdir -p "$TMP/x" && tar -xJf "$TMP/$FILE" -C "$TMP/x"
# the libraries must be the ones the bundle says it holds
for lib in libbt4dec.so libdatachannel.so; do
    want=$(sed -n "s/^$lib sha256 //p" "$TMP/x/BUILD-INFO.txt")
    have=$(sha256sum "$TMP/x/jniLibs/arm64-v8a/$lib" | cut -d' ' -f1)
    [ -n "$want" ] && [ "$want" = "$have" ] || { echo "::error::$lib in $FILE is not the one its BUILD-INFO.txt names"; exit 1; }
done
mkdir -p "$APP/jniLibs/arm64-v8a" "$APP/assets"
cp "$TMP/x/jniLibs/arm64-v8a/libbt4dec.so" "$TMP/x/jniLibs/arm64-v8a/libdatachannel.so" "$APP/jniLibs/arm64-v8a/"
rm -rf "$APP/assets/bt4dec" && cp -R "$TMP/x/assets/bt4dec" "$APP/assets/bt4dec"
{ echo; cat "$TMP/x/licenses-bt4dec.txt"; } >> "$APP/assets/licenses.txt"
echo "BT4decomp engine: $(head -1 "$TMP/x/BUILD-INFO.txt")"
rm -rf "$TMP"
