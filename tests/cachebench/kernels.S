/* tests/cachebench kernels (main.c; docs/ICACHE.md section 6): code and loads whose cost on jtcps3 sets the
   cache-bypass model's constants. All are position independent (only branches inside a kernel, no literals), so
   main.c runs each through SIMM 1's cached address (0x06xxxxxx) or its cache-through mirror (| 0x20000000).
   Arguments: r4 count, r5 address. r6 / r7 are scratch for the filler instructions. */
    .text

/* 4,096 instructions of straight-line code (8 KB, twice the cache): run cached, every 16-byte line misses on every
   call; run through the mirror, every 32-bit word is one bus read */
    .balign 16
    .global _k_lin
_k_lin:
    .rept   4094
    add     r6, r7
    .endr
    rts
    nop

/* 2 KB of straight-line code (1,024 instructions): fits the cache with room to spare */
    .balign 16
    .global _k_hot
_k_hot:
    .rept   1022
    add     r6, r7
    .endr
    rts
    nop

/* 512 lines of which only the first 2 instructions run (BRA over the other 6): a cached run reads every line whole for
   2 instructions; the mirror reads one word a line */
    .balign 16
    .global _k_sparse
_k_sparse:
    .rept   511
    bra     1f
    nop
    .rept   6
    add     r6, r7
    .endr
1:
    .endr
    rts
    nop

/* r4 iterations of 4 loads 16 bytes apart (one a line), then r5 += 64 */
    .balign 16
    .global _k_ld16
_k_ld16:
1:  mov.l   @r5, r0
    mov.l   @(16,r5), r1
    mov.l   @(32,r5), r2
    mov.l   @(48,r5), r3
    dt      r4
    bf/s    1b
    add     #64, r5
    rts
    nop

/* r4 iterations of 16 sequential 32-bit loads (64 bytes: 4 lines) */
    .balign 16
    .global _k_ld4
_k_ld4:
1:
    .rept   16
    mov.l   @r5+, r0
    .endr
    dt      r4
    bf      1b
    rts
    nop
