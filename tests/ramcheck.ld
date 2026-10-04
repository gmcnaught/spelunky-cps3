/* tests/*: main RAM check, added to every SH-2 link (-Wl,-T,../ramcheck.ld). The SDK (link_simm.ld) puts .data and .bss
   at 0x02000000 and the stack top at 0x02080000 (512 KB of main RAM): .bss must end 32 KB short of it, the room
   left for the stack (tests/game at PIN_MAX 1792: 50 KB is left, docs/DRAW.md section 6) */
ASSERT(__bss_end <= 0x02080000 - 0x8000, "main RAM: .bss leaves under 32 KB for the stack (PIN_MAX / EXT_MAX too large)")
