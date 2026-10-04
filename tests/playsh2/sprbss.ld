/* tests/playsh2: the play loop's and the generator's largest arrays in sprite RAM (0x04000000-0x0407ffff, 512 KB;
   unused: no video), so the instances fit main RAM (512 KB): the generator's instance list (inst.c W), the
   collision tree (pcol.c rn, er, sn, epass) and the tile list (gen.c gtiles). main.c clears it before each job. */
SECTIONS
{
    .sprbss 0x04000000 (NOLOAD) : {
        __sprbss_start = .;
        *(.bss.W) *(.bss.rn) *(.bss.er) *(.bss.sn) *(.bss.epass) *(.bss.gtiles)
        . = ALIGN(4);
        __sprbss_end = .;
    }
}
INSERT BEFORE .bss;
