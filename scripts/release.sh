#!/bin/sh
# The release: tests/game with PLAY=1 (the cabinet's coin / Start and controls, a random seed, enemies on, level 1) as a
# MiSTer jtcps3 set, spelunky.zip + "Spelunky Classic Arcade.mra", copied to build/release/. Reads the committed
# build/gen and build/snd, so refs/ is not needed (after a change to tools/ or refs/: make gen, commit build/gen and
# build/snd). src/game is taken at GAME_REV (default HEAD) with PIN_MAX 1792 and the unity TU, as
# scripts/game_check.sh does. Linux and macOS (.github/workflows/build.yml runs it).
#   scripts/release.sh        -> build/release/spelunky.zip, build/release/Spelunky Classic Arcade.mra
set -e
cd "$(dirname "$0")/.."
T=tests/game; G=$T/build/g; R=build/release
rm -rf "$G" "$T/build/release" "$R"; mkdir -p "$G" "$R"
git archive "${GAME_REV:-HEAD}" src/game | tar -x -C "$G" --strip-components=2
cp build/gen/objects.[ch] build/gen/gentables.[ch] build/gen/playtables.[ch] "$G/"
sed "s/^#define PIN_MAX [0-9][0-9]*\$/#define PIN_MAX 1792/" "$G/play.h" > "$G/play.tmp" && mv "$G/play.tmp" "$G/play.h"
grep -q "^#define PIN_MAX 1792\$" "$G/play.h"
scripts/unity.sh "$G"
touch "$G/stamp"
scripts/dmake.sh $T OUT=build/release PLAY=1 mister
cp "$T"/build/release/mister/* "$R/"
ls -l "$R"
