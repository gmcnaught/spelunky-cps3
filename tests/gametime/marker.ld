/* tests/gametime: the results (hooks.c M, 6,384 bytes) in sprite RAM past tests/game's area A (sprbss.ld), not in
   main RAM: the game program's .bss leaves no room for them above the 32 KB stack reserve (tests/ramcheck.ld).
   main_boot clears area A only up to __sprbss_a_end; hooks.c clears M. scripts/lua/gametime.lua reads them here. */
SECTIONS
{
    .gtmark 0x0402e000 (NOLOAD) : {
        KEEP(*(.gtmark))
        . = ALIGN(4);
        __gtmark_end = .;
    }
}
INSERT BEFORE .bss;
ASSERT(__sprbss_a_end <= 0x0402e000, "tests/game's sprbss_a reaches gametime's results (0x0402e000)")
ASSERT(__gtmark_end <= 0x04030000, "gametime's results overlap src/draw's sublists (0x30000-0x3ffff)")
