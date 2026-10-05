#!/bin/sh
# Untranslated-code survey (docs/CONTENT.md): build/host/untrsurvey (test/host/untrsurvey.c) over levels 1-16, SEEDS
# seeds each (default 30: 480 levels), 1,500 steps of random cabinet inputs per level, every PUNTR site recorded.
# Passes, each a file in build/untr/:
#   play.txt      the cabinet's play (no input after the player's death)
#   god.txt       the developer option INVINCIBLE (the player explores longer; not the cabinet's play)
#   item_<x>.txt  the same as play with an item held from the start (pickupItem) or equipment (has*): the items
#                 the player can buy or find
# and the table build/untr/table.txt: per code, C site and object (object/other for a collision), the levels of
# 480 that reach it in each pass (item: the levels where any item pass reaches it, then those items).
#   scripts/untr_survey.sh [--quick]      (--quick: 5 seeds)
#   build/host/untrsurvey --levels L --seeds S --route tests/routes/<name>.txt   (one run as a route)
set -e
cd "$(dirname "$0")/.."
SEEDS=${SEEDS:-30}
[ "${1:-}" = --quick ] && SEEDS=5
make -s -C test/host "$PWD/build/host/untrsurvey"
O=build/untr
mkdir -p $O
rm -f $O/item_*.txt
S="--levels 1-16 --seeds 1-$SEEDS"
build/host/untrsurvey $S > $O/play.txt
build/host/untrsurvey $S --god 1 > $O/god.txt
for it in pickupItem=Rock pickupItem=Jar pickupItem=Skull pickupItem=Fish~Bone pickupItem=Arrow pickupItem=Machete \
          pickupItem=Mattock pickupItem=Pistol pickupItem=Web~Cannon pickupItem=Teleporter pickupItem=Shotgun \
          pickupItem=Bow,arrows=6 pickupItem=Flare pickupItem=Sceptre pickupItem=Key \
          hasJetpack=1 hasCape=1 hasParachute=1 hasMitt=1 hasGloves=1 hasSpringShoes=1 hasSpikeShoes=1 hasKapala=1 \
          hasAnkh=1 hasCompass=1 hasStickyBombs=1 hasUdjatEye=1 hasCrown=1; do
  n=$(echo "$it" | sed 's/[=,~]/_/g')
  build/host/untrsurvey $S --global "$it" | sed "s/^U /U $n /" > $O/item_$n.txt
done
for f in $O/play.txt $O/god.txt; do
  n=$(grep -c '^E' $f); [ "$n" -eq $((16 * SEEDS)) ] || echo "$f: $n of $((16 * SEEDS)) runs ended (a run failed: see stderr)"
done
# per pass: distinct (level, seed) per code + site + object
cat $O/play.txt | awk '$1=="U"{print "play", $4, $6, $5, $2 "/" $3}' > $O/rows.txt
cat $O/god.txt | awk '$1=="U"{print "god", $4, $6, $5, $2 "/" $3}' >> $O/rows.txt
cat $O/item_*.txt | awk '$1=="U"{print "item:" $2, $5, $7, $6, $3 "/" $4}' >> $O/rows.txt
awk -v N=$((16 * SEEDS)) '
  { key = $2 " " $3 " " $4; p = $1; it = ""; if (p ~ /^item:/) { it = substr(p, 6); p = "item" }
    if (!((key, p, $5) in seen)) { seen[key, p, $5] = 1; cnt[key, p]++ }
    if (it != "" && !((key, it) in iseen)) { iseen[key, it] = 1; items[key] = items[key] (items[key] == "" ? "" : ",") it }
    keys[key] = 1 }
  END { for (k in keys) { split(k, a, " ")
          printf "%-6s %-18s %-36s %5d %5d %5d  %s\n", a[1], a[2], a[3], cnt[k, "play"], cnt[k, "god"], cnt[k, "item"], items[k] } }
' $O/rows.txt | sort -k4,4nr -k5,5nr -k6,6nr > $O/table.body
printf "%-6s %-18s %-36s %5s %5s %5s  %s\n" code site object play god item "items (levels of $((16 * SEEDS)) per pass; item: any item pass)" \
  | cat - $O/table.body > $O/table.txt
rm -f $O/table.body
cat $O/table.txt
