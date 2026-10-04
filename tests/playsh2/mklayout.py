#!/usr/bin/env python3
"""tests/playsh2 code layout for jtcps3 (its SH-2 fetches code from SIMM 1 through a 4 KB cache): a linker script,
the SDK's sdk/link_simm.ld with the hottest functions (built with -ffunction-sections: section .text.<name>) first
in SIMM 1, in profile order.
   mklayout.py <hot.txt> <out.ld> [max function bytes] [coverage %]
hot.txt: "<nm name> <bytes> <PC samples>" per function, hottest first (a PROF run's samples; tests/playsh2/hot.txt
is HEAD 4fe74fc's, scripts/playsh2_check.sh PROF=512 PROF_SKIP=4). Functions up to max bytes (default 2048) are
taken until they hold coverage % (default 92) of all samples; the rest of the code follows in link order."""
import sys

src, out = sys.argv[1], sys.argv[2]
maxb = int(sys.argv[3]) if len(sys.argv) > 3 else 2048
cov = float(sys.argv[4]) if len(sys.argv) > 4 else 92.0
rows = [l.split() for l in open(src) if l.strip()]
total = sum(int(r[2]) for r in rows)
pick, got, size = [], 0, 0
for name, b, n in rows:
    if got * 100.0 / total >= cov:
        break
    if int(b) > maxb:
        continue
    pick.append(name[1:] if name.startswith('_') else name)
    got += int(n)
    size += int(b)
hot = '\n'.join('        *(.text.%s)' % n for n in pick)
ld = '''/* tests/playsh2/mklayout.py: sdk/link_simm.ld with %d hot functions (%d bytes, %.1f%% of the PC samples) first in
   SIMM 1 */
OUTPUT_FORMAT("elf32-sh")
ENTRY(start)
MEMORY
{
    rom (rx) : ORIGIN = 0x00000000, LENGTH = 0x80000
    simm (rx) : ORIGIN = 0x06000000, LENGTH = 0x800000
    ram (rw) : ORIGIN = 0x02000000, LENGTH = 0x80000
}
__stack_top = 0x02080000;
SECTIONS
{
    .boot : {
        KEEP(*(.vectors))
        *(.boot)
        . = ALIGN(4);
    } > rom
    .text : {
%s
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > simm
    .trace (NOLOAD) : {
        KEEP(*(.trace))
        . = ALIGN(4);
    } > ram
    .data : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > ram AT > simm
    __data_load = LOADADDR(.data);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ram
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
''' % (len(pick), size, got * 100.0 / total, hot)
open(out, 'w').write(ld)
print('%d functions, %d bytes, %.1f%% of the samples' % (len(pick), size, got * 100.0 / total))
