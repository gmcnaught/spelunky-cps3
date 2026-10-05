/* tests/playsh2 JT=1 (scripts/playsh2_jt.sh): the arrays used only while a level is generated go to sprite RAM
   (0x0a000.., below the SDK's prebuilt-sublist end; nothing is drawn): the R-tree nodes (pcol.c rn: the play-time
   collisions use the grid), the tile list (gtiles) and the generator's lists (inst.c). Main RAM (512 KB) then holds
   the play state with the stack room tests/ramcheck.ld checks; generation timings include these sprite-RAM accesses.
   main.c (ram_init) clears the area. */
SECTIONS
{
    .sprbss 0x0400a000 (NOLOAD) : {
        __sprbss_start = .;
        *(.bss.rn) *(.bss.gtiles) *(.bss.with_pool) *(.bss.gw_inext) *(.bss.gw_iprev) *(.bss.ghome) *(.bss.gw_ohead)
        *(.bss.gw_otail) *(.bss.gw_live)
        /* the static-family index (pworld.c; read by a few queries a step, written when an indexed entry changes) */
        *(.bss.xcnt) *(.bss.xmask) *(.bss.xisfar) *(.bss.xond) *(.bss.xx0) *(.bss.xy0) *(.bss.xx1) *(.bss.xy1)
        *(.bss.xdnext)
        . = ALIGN(4);
        __sprbss_end = .;
    }
}
INSERT BEFORE .bss;
ASSERT(__sprbss_end <= 0x04038000, "jtcold: past the free sprite RAM")
