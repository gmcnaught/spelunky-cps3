/* binary64 add and subtract for the SH-2 in assembly (src/sh2/softfp.c has the rest and the general C path).
 *
 * Fast path: both operands normal, not the same magnitude with opposite signs closer than 2 binades apart (that
 * cancellation, zeros, subnormals, infinities and NaNs go to softfp.c's C add, softfp_add_c, with the operands
 * as they came, or swapped, which is the same for finite normal operands). The larger magnitude a, the smaller b
 * aligned to it as a 96-bit value (hi : lo : x), x holding the bits shifted out, exactly while the shift d is at
 * most 32 and with a sticky bit below them beyond; then the 96-bit sum or difference, one normalising shift (the
 * difference loses at most one leading bit when d >= 2), round to nearest even on x, and the exponent kept in
 * place (sign | exponent field) so no shifts are needed to pack. Variable shifts are dmulu.l products by 2^k (the
 * SH-2 shifts only by constants). Results equal softfp_add_c's / IEEE's: tests/softfp.
 *
 * Calling convention (GCC SH, no FPU): a in r4 (high word) : r5, b in r6 : r7, the result in r0 : r1; r0-r7,
 * MACH, MACL free; r8 saved. SOFTFP_SFNAMES: the entries are named sf_adddf3 / sf_subdf3 (tests/softfp). */
#ifdef SOFTFP_SFNAMES
#define ADD _sf_adddf3
#define SUB _sf_subdf3
#else
#define ADD ___adddf3
#define SUB ___subdf3
#endif

    .text
    .align 2
    .global SUB
SUB:                                    /* a - b = a + (-b) (a NaN b too: fp-bit flips its sign) */
    mov.l   .Lk8, r0
    bra     ADD
    xor     r0, r6

.Lslow:                                 /* the C path (kept before ADD: the entry tests reach it by bt) */
    mov.l   .Lc, r0
    jmp     @r0
    nop

    .align 2
    .global ADD
ADD:
    mov.l   .Lk7ff, r1                  /* 0x7ff00000 */
    mov     r4, r2
    and     r1, r2                      /* a's exponent field */
    mov     r6, r3
    and     r1, r3                      /* b's */
    cmp/eq  r1, r2
    bt      .Lslow                      /* inf, NaN */
    cmp/eq  r1, r3
    bt      .Lslow
    tst     r2, r2
    bt      .Lslow                      /* zero, subnormal */
    tst     r3, r3
    bt      .Lslow
    /* the larger magnitude first */
    cmp/hi  r3, r2
    bt      .Lord
    cmp/eq  r3, r2
    bf      .Lswap
    mov.l   .Lkm, r0                    /* the same exponent: the significands decide */
    mov     r4, r1
    and     r0, r1
    and     r6, r0
    cmp/hi  r0, r1
    bt      .Lord
    cmp/eq  r0, r1
    bf      .Lswap
    cmp/hs  r7, r5
    bt      .Lord
.Lswap:
    mov     r4, r0
    mov     r6, r4
    mov     r0, r6
    mov     r5, r0
    mov     r7, r5
    mov     r0, r7
    mov     r2, r0
    mov     r3, r2
    mov     r0, r3
.Lord:
    /* r2 = a's exponent field, r3 = b's; d = their difference */
    mov     r2, r1
    sub     r3, r1
    shlr16  r1
    shlr2   r1
    shlr2   r1                          /* d */
    mov     r4, r3
    xor     r6, r3                      /* bit 31: the signs differ */
    cmp/pz  r3
    bt      1f
    mov     #1, r0                      /* opposite signs, d <= 1: cancellation, the C path */
    cmp/hi  r0, r1
    bf      .Lslow
1:  mov.l   .Lkse, r0                   /* 0xfff00000 */
    and     r4, r0
    mov     r0, r2                      /* r2 = sign | exponent field of the result (a's) */
    mov.l   r8, @-r15
    mov.l   .Lkfr, r0                   /* 0x000fffff */
    and     r0, r4
    and     r0, r6
    mov.l   .Lkim, r0                   /* 0x00100000: the implicit bit */
    or      r0, r4                      /* a: r4 : r5 */
    or      r0, r6                      /* b: r6 : r7 */
    tst     r1, r1
    bt/s    .Lal
    mov     #0, r8                      /* x */
    mov     #31, r0
    cmp/hi  r0, r1
    bt      .Lbig
    /* 1 <= d <= 31: (b : 0) >> d with 2^(32 - d) */
    mov     #32, r8
    sub     r1, r8
    shll2   r8
    mova    .Lp2, r0
    mov.l   @(r0, r8), r0
    dmulu.l r0, r7
    sts     macl, r8                    /* x = lo << (32 - d) */
    sts     mach, r7                    /* lo >> d */
    dmulu.l r0, r6
    sts     macl, r0                    /* hi << (32 - d) */
    or      r0, r7
    bra     .Lal
    sts     mach, r6                    /* hi >> d */
.Lbig:
    mov     #63, r0
    cmp/hi  r0, r1
    bt      .Ltiny
    add     #-32, r1                    /* e = d - 32 */
    tst     r1, r1
    bf      2f
    mov     r7, r8                      /* d = 32: x = lo, lo = hi, hi = 0 */
    mov     r6, r7
    bra     .Lal
    mov     #0, r6
2:  mov     #32, r8
    sub     r1, r8
    shll2   r8
    mova    .Lp2, r0
    mov.l   @(r0, r8), r0               /* 2^(32 - e) */
    dmulu.l r0, r7
    sts     mach, r8                    /* lo >> e */
    sts     macl, r1                    /* lo << (32 - e): below x, the sticky bit */
    dmulu.l r0, r6
    sts     macl, r0                    /* hi << (32 - e) */
    or      r0, r8
    tst     r1, r1
    movt    r0
    xor     #1, r0
    or      r0, r8                      /* sticky */
    sts     mach, r7                    /* lo = hi >> e */
    bra     .Lal
    mov     #0, r6
.Ltiny:                                 /* d >= 64: b is a sticky bit */
    mov     #1, r8
    mov     #0, r7
    mov     #0, r6
.Lal:
    cmp/pz  r3
    bf      .Lsub
    clrt                                /* same signs: (a : 0) + (b : x) */
    addc    r7, r5
    addc    r6, r4
    mov.l   .Lki2, r0                   /* 0x00200000 */
    tst     r0, r4
    bt      .Lround
    shlr    r4                          /* a carry: one place right, the sticky bit kept */
    rotcr   r5
    rotcr   r8
    movt    r0
    or      r0, r8
    mov.l   .Lkim, r0
    add     r0, r2                      /* exponent + 1 */
    mov.l   .Lk7ff, r0
    mov     r2, r1
    and     r0, r1
    cmp/eq  r0, r1
    bt      .Linf
    bra     .Lround
    nop
.Lsub:
    clrt                                /* (a : 0) - (b : x) */
    negc    r8, r8
    subc    r7, r5
    subc    r6, r4
    mov.l   .Lkim, r0
    tst     r0, r4
    bf      .Lround
    shll    r8                          /* the leading bit lost (d >= 2: one place) */
    rotcl   r5
    rotcl   r4
    sub     r0, r2                      /* exponent - 1 (stays >= 1) */
.Lround:                                /* nearest even: x > 2^31, or x = 2^31 and lo odd */
    mov.l   .Lk8, r0
    cmp/hi  r0, r8
    bt      .Lup
    cmp/eq  r0, r8
    bf      .Lpack
    mov     r5, r0
    tst     #1, r0
    bt      .Lpack
.Lup:
    mov     #0, r0
    sett
    addc    r0, r5
    addc    r0, r4
    mov.l   .Lki2, r0
    cmp/eq  r0, r4
    bf      .Lpack
    mov.l   .Lkim, r0                   /* rounded up to 2^53: 2^52, exponent + 1 */
    mov     r0, r4
    add     r0, r2
    mov.l   .Lk7ff, r0
    mov     r2, r1
    and     r0, r1
    cmp/eq  r0, r1
    bt      .Linf
.Lpack:                                 /* (sign | exponent) - implicit bit + significand */
    mov.l   .Lkim, r0
    sub     r0, r2
    mov     r2, r0
    add     r4, r0
    mov     r5, r1
    rts
    mov.l   @r15+, r8
.Linf:                                  /* overflow: r2 = sign | 0x7ff00000 */
    mov     r2, r0
    mov     #0, r1
    rts
    mov.l   @r15+, r8

    .align 2
.Lk8:   .long 0x80000000
.Lk7ff: .long 0x7ff00000
.Lkm:   .long 0x7fffffff
.Lkse:  .long 0xfff00000
.Lkfr:  .long 0x000fffff
.Lkim:  .long 0x00100000
.Lki2:  .long 0x00200000
.Lc:    .long _softfp_add_c
.Lp2:   .long 1, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7, 1 << 8, 1 << 9, 1 << 10, 1 << 11
        .long 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22
        .long 1 << 23, 1 << 24, 1 << 25, 1 << 26, 1 << 27, 1 << 28, 1 << 29, 1 << 30, 1 << 31
