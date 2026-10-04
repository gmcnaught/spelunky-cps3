/* tests/playsh2: the generator's instance list (inst.c W, 168 KB, used during generation and level load only) in
   sprite RAM (0x04040000-0x0407ffff; unused: no video), so the play loop's instances fit main RAM */
SECTIONS
{
    .sprbss 0x04040000 (NOLOAD) : { *(.bss.W) }
}
INSERT BEFORE .bss;
