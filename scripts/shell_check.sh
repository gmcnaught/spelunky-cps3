#!/bin/sh
# tests/shell: host checks (input / credit / high-score logic), then the shell's main loop in MAME driven by
# scripts/lua/shellin.lua, its step log checked by tools/shellcheck.py.     scripts/shell_check.sh
set -e
cd "$(dirname "$0")/.."
(cd tests/shell && make -s host)
scripts/dmake.sh tests/shell >/dev/null
B=tests/shell/build; O=$B/run; rm -rf "$O"; mkdir -p "$O/w"
A=$(awk '$2 == "_shlog" {print $1}' "$B/main.map")
# two runs on one EEPROM (MAME's nvram directory): the first from a blank EEPROM, the second reads what it stored
for run in 1 2; do
  SHLOG=$A mame sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -video none -seconds_to_run 60 \
    -cfg_directory "$O/w/cfg" -nvram_directory "$O/w/nvram" -snapshot_directory "$O/snap" -diff_directory "$O/w/diff" \
    -state_directory "$O/w/sta" -inipath "$O/w" -autoboot_script scripts/lua/shellin.lua >"$O/mame$run.log" 2>&1 || true
done
python3 tools/shellcheck.py "$O/mame1.log" "$O/mame2.log"
# the settings screen: free play on and 2 coins a credit stored (EEPROM word 27), a game begun without a coin
O2=$O/menu; mkdir -p "$O2/w"
SHLOG=$A mame sfiii3na -rompath "$B/mame" -skip_gameinfo -nothrottle -sound none -video none -seconds_to_run 60 \
  -cfg_directory "$O2/w/cfg" -nvram_directory "$O2/w/nvram" -snapshot_directory "$O2/snap" -inipath "$O2/w" \
  -autoboot_script scripts/lua/shellmenu.lua >"$O2/mame.log" 2>&1 || true
python3 - "$O2/mame.log" "$O2/w/nvram/sfiii3na/eeprom" <<'PY'
import struct, sys
b = [l.split() for l in open(sys.argv[1]) if l.startswith('BEGIN')]
w = struct.unpack('<I', open(sys.argv[2], 'rb').read()[27 * 4:28 * 4])[0]
ok = len(b) == 1 and b[0][2] == '0' and w == 0x201
print(('ok   ' if ok else 'FAIL ') + f'settings screen: free play + 2 coins stored (word 27 = {w:#x}), game begun without a coin {b}')
sys.exit(0 if ok else 1)
PY
