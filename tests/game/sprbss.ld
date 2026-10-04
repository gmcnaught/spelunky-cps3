/* tests/game: large arrays in sprite RAM areas the display does not use (MAME only: on jtcps3 CPU access to
   sprite RAM is not checked for this). Display use (sdk cps3v.h): main list 0x00000-0x01fff, the frame's sublists
   0x02000-0x09fff, tilemaps 0..3 in units 0x40-0x4f (0x40000-0x4ffff). Free: 0x0a000-0x3ffff (216 KB) and
   0x50000-0x7ffff (192 KB). main.c (main_boot) clears both before use. */
SECTIONS
{
    .sprbss_a 0x0400a000 (NOLOAD) : {
        __sprbss_a_start = .;
        *(.bss.W) *(.bss.gtiles) *(.bss.drawcache)
        . = ALIGN(4);
        __sprbss_a_end = .;
    }
    .sprbss_b 0x04050000 (NOLOAD) : {
        __sprbss_b_start = .;
        *(.bss.rn) *(.bss.er) *(.bss.sn) *(.bss.epass) *(.bss.drawmaps)
        . = ALIGN(4);
        __sprbss_b_end = .;
    }
}
INSERT BEFORE .bss;
ASSERT(__sprbss_a_end <= 0x04040000, "sprbss_a overlaps the tilemaps")
ASSERT(__sprbss_b_end <= 0x04080000, "sprbss_b past sprite RAM")
