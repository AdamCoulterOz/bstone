#!/usr/bin/env bash
#
# Download the free Blake Stone: Aliens of Gold (shareware) game data that
# bstone needs at runtime, into ./data/.
#
# Only the shareware episode is freely redistributable. The full Aliens of Gold
# and Planet Strike data are commercial (GOG/Steam) and are NOT downloaded here;
# if you own them, drop their *.BS6 / *.VSI files into ./data/ instead.
#
# Source: the bstone author's official repack (AOG v3.0 shareware), whose 10
# *.BS1 files match the engine's built-in SHA1 manifest.
#
set -euo pipefail

cd "$(dirname "$0")"

URL="https://bibendovsky.github.io/bstone/files/official/repack/bs_aog_v3_0_sw.zip"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading Aliens of Gold v3.0 shareware data..."
curl -fSL --retry 2 -o "$TMP/aog_sw.zip" "$URL"

mkdir -p data
unzip -o "$TMP/aog_sw.zip" -d "$TMP/x" >/dev/null

# Keep only the game-data resource files (drop the DOS executables/docs).
cp "$TMP"/x/*.BS1 data/
chmod u+w data/*.BS1

echo "Game data in ./data/:"
ls -1 data/*.BS1
