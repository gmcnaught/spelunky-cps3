#!/bin/sh
# src/sh2/softfp.c (the soft-float replacing libgcc's fp-bit): the host test against the host's FPU, then the SH-2
# test in MAME against fp-bit on the same cases; the SH-2's hash per operation must equal the host's (= the FPU's
# results) and no raw result may differ from fp-bit's. Clocks per call (MAME) for both.
#   [CASES=500000] [OPMASK=<bits>] [SECONDS_TO_RUN=20000] scripts/softfp_check.sh
# OPMASK: only the operations whose bit is set (cases.h's order; e.g. 0x10001000 = truncdfsf2 and piece with
# CASES=1000000000: the bulk run); the enumerated sets (trunc_edge, piece_edge, piece_host) have their own sizes.
# piece_host replays tests/softfp/build/piece_tuples.h when it exists (the host runs' rubblepiece_step operands).
set -e
cd "$(dirname "$0")/.."
T=tests/softfp; B=$T/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
N=${CASES:-500000}
PT=; [ -f $B/piece_tuples.h ] && PT="-DPIECE_TUPLES -I$B"
cc -std=c99 -O2 -ffp-contract=off -Wall -Wextra -DSOFTFP_HOST $PT -I$T -o $B/host $T/host.c src/sh2/softfp.c
$B/host $N ${OPMASK:-0xffffffffffffffff} > "$O/host.txt" || true
scripts/dmake.sh $T -B CASES=$N OPMASK=${OPMASK:-0xffffffffffffffffull} >/dev/null    # -B: the SH-2 build must use this run's CASES
SOFTFP_OUT="$O/sh2.txt" scripts/mame.sh sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -video none \
  -seconds_to_run ${SECONDS_TO_RUN:-20000} -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" -snapshot_directory "$O/w/snap" \
  -diff_directory "$O/w/diff" -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/softfp.lua \
  >"$O/mame.log" 2>&1 || true
python3 - "$O/host.txt" "$O/sh2.txt" <<'PY'
import sys
names = []; host = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) >= 7 and f[2] == 'cases,':
        host[f[0]] = (int(f[1]), int(f[3]), f[6]); names.append(f[0])
tot = bad = hbad = 0
print('%-12s %9s %8s %6s %9s %9s %7s' % ('op', 'cases', 'vs fpbit', 'hash', 'fp-bit', 'softfp', 'speedup'))
for l in open(sys.argv[2]):
    f = l.split()
    op, n, nd, h, cf, cs, cl = int(f[1]), int(f[2]), int(f[3]), f[4], int(f[5]), int(f[6]), int(f[7])
    name = names[op]
    hn, hd, hh = host[name]
    ok = 'equal' if hh == h and hn == n and hd == 0 else 'DIFF'
    fpb, sfc = (cf - cl) / 256.0, (cs - cl) / 256.0
    print('%-12s %9d %8d %6s %9.1f %9.1f %6.1fx%s' % (name, n, nd, ok, fpb, sfc, fpb / sfc if sfc > 0 else 0,
          ''.join('  [' + ' '.join(f[8 + 3 * k:11 + 3 * k]) + ']' for k in range((len(f) - 8) // 3))))
    tot += n; bad += nd; hbad += ok != 'equal'
print('total %d cases: %d differ from fp-bit, %d operations with a hash unequal to the host FPU\'s' % (tot, bad, hbad))
print('(clocks per call in MAME, loop subtracted; game-like normal operands)')
PY
