#!/usr/bin/env python3
"""tests/playsh2 linker script with code and data read past the SH-2 cache (docs/ICACHE.md): the SDK's
sdk/link_simm.ld, built with -ffunction-sections / -fdata-sections (the Makefile's LAYOUT=), where the functions and
constant tables named in <nc.txt> are linked at SIMM 1's cache-through mirror (0x26000000 + offset, loaded at
0x06000000 + offset) and the .bss arrays named there at main RAM's (0x22000000 + offset). Every access to them then
goes past the cache (SH7604: A31-29 = 001), so they never fill a line, while the code that calls or reads them is
unchanged (calls and addresses go through literals, and the moved functions' own branches and literals are
PC-relative). The image keeps one SIMM 1 layout: .nctext first, then .text and .data's load image.
   mknc.py <nc.txt> <out.ld> [--shadow]
--shadow: the same layout with the moved sections at their cached addresses (bit 29 clear), for MAME traces:
MAME 0.264's debugger does not disassemble code at 0x26000000 (the trace shows no loads or stores there), so
scripts/jtcost_native.sh traces the shadow build and tools/jtbypass.py charges the moved functions and arrays as
uncached (--nc / --ncd from this file).
nc.txt: one entry a line, "f <function>" (an nm name without the leading underscore, clones included: foo.part.0),
"r <object>" (a const table, .rodata.<object>), "b <object>" (a .bss array, .bss.<object>); # starts a comment.
Empty: the same layout as LAYOUT's plain link (a baseline with -ffunction-sections)."""
import sys

src, out = sys.argv[1], sys.argv[2]
U = 0 if '--shadow' in sys.argv[3:] else 0x20000000
fs, rs, bs = [], [], []
for l in open(src):
    l = l.split('#')[0].split()
    if len(l) != 2: continue
    {'f': fs, 'r': rs, 'b': bs}[l[0]].append(l[1])
txt = '\n'.join('        *(.text.%s)' % n for n in fs) + '\n' + '\n'.join('        *(.rodata.%s)' % n for n in rs)
bss = '\n'.join('        *(.bss.%s)' % n for n in bs)
ld = '''/* tests/playsh2/mknc.py: sdk/link_simm.ld with %d functions and %d const tables at SIMM 1's cache-through mirror
   and %d .bss arrays at main RAM's (docs/ICACHE.md) */
OUTPUT_FORMAT("elf32-sh")
ENTRY(start)
MEMORY
{
    rom (rx) : ORIGIN = 0x00000000, LENGTH = 0x80000
}
__stack_top = 0x02080000;
SECTIONS
{
    .boot : {
        KEEP(*(.vectors))
        *(.boot)
        . = ALIGN(4);
    } > rom
    .nctext %#x : AT(0x06000000) {
%s
        . = ALIGN(16);
    }
    .text (0x06000000 + SIZEOF(.nctext)) : AT(0x06000000 + SIZEOF(.nctext)) {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    }
    .trace 0x02000000 (NOLOAD) : {
        KEEP(*(.trace))
        . = ALIGN(4);
    }
    .data (ADDR(.trace) + SIZEOF(.trace)) : AT(LOADADDR(.text) + SIZEOF(.text)) {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    }
    __data_load = LOADADDR(.data);
    __bss_start = ALIGN(ADDR(.data) + SIZEOF(.data), 16);
    .ncbss (__bss_start | %#x) (NOLOAD) : {
%s
        . = ALIGN(16);
    }
    .bss (__bss_start + SIZEOF(.ncbss)) (NOLOAD) : {
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
''' % (len(fs), len(rs), len(bs), 0x06000000 | U, txt, U, bss)
open(out, 'w').write(ld)
