/* Soft-float entry points in SH-2 assembly (src/sh2/softfp.c has the rest and the general C paths): binary64 add
 * and subtract, binary32 add and subtract, the binary64 compares, binary64 to binary32 (__truncdfsf2); a debris
 * piece's y move on them (softfp_piece_y).
 *
 * binary64 add: a normal operand plus +-0 returns it; two normal operands (except opposite signs within one binade
 * when the larger exponent is below 55, where the result could be subnormal) take the fast path; zeros with zeros,
 * subnormals, infinities and NaNs go to softfp.c's C add, softfp_add_c, with the operands as they came, or
 * swapped, which is the same for finite normal operands. The larger magnitude a, the smaller b aligned to it as a
 * 96-bit value (hi : lo : x), x holding the bits shifted out, exactly while the shift d is at most 32 and with a
 * sticky bit below them beyond; then the 96-bit sum or difference, normalised (one place right after a carry; left
 * by bytes then single places after a cancellation), round to nearest even on x, and the exponent kept in place
 * (sign | exponent field) so no shifts are needed to pack. Variable shifts are dmulu.l products by 2^k (the SH-2
 * shifts only by constants). binary32 add: the same scheme in one word (6 guard bits, a sticky bit); NaNs,
 * infinities, subnormals and results that could underflow go to softfp_addsf3_c. Compares: fp-bit's values (-1 /
 * 0 / 1; with a NaN 1, or -1 for gt / ge) from the bit patterns. Results equal softfp.c's C / IEEE's: tests/softfp.
 *
 * Calling convention (GCC SH, no FPU): a in r4 (high word) : r5, b in r6 : r7, the result in r0 : r1; r0-r7,
 * MACH, MACL free; r8 saved. A float travels in r4 / r5, returned in r0. SOFTFP_SFNAMES: the entries are named
 * sf_* (tests/softfp, beside libgcc's). */
#ifdef SOFTFP_SFNAMES
#define N(n) _sf_##n
#else
#define N(n) ___##n
#endif
#define ADD N(adddf3)
#define SUB N(subdf3)

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
.Laz:                                   /* a's exponent 0: a = +-0 and b normal gives b */
    tst     r3, r3
    bt      .Lslow
    mov     r4, r0
    add     r0, r0
    or      r5, r0
    tst     r0, r0
    bf      .Lslow
    mov     r6, r0
    rts
    mov     r7, r1
.Lbz:                                   /* b's exponent 0, a normal: b = +-0 gives a */
    mov     r6, r0
    add     r0, r0
    or      r7, r0
    tst     r0, r0
    bf      .Lslow
    mov     r4, r0
    rts
    mov     r5, r1

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
    bt      .Laz                        /* zero, subnormal */
    tst     r3, r3
    bt      .Lbz
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
    mov     #1, r0                      /* opposite signs, d <= 1: a cancellation of up to 53 places; */
    cmp/hi  r0, r1
    bt      1f
    mov.l   .Lk55, r0                   /* the C path when a's exponent is below 55 (a subnormal result */
    cmp/hs  r0, r2                      /* possible) */
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
    mov     r4, r0                      /* x - x = +0 */
    or      r5, r0
    or      r8, r0
    tst     r0, r0
    bt      .Lzero
.Lnorm:                                 /* the leading bit back to bit 20 of hi (exponent stays >= 1) */
    mov.l   .Lkim, r0
    tst     r0, r4
    bf      .Lround
    mov.l   .Lk1fe, r1                  /* bits 20..13 of hi clear: a byte at once */
    tst     r1, r4
    bf      3f
    shll8   r4
    mov     r5, r1
    shlr16  r1
    shlr8   r1
    or      r1, r4
    shll8   r5
    mov     r8, r1
    shlr16  r1
    shlr8   r1
    or      r1, r5
    shll8   r8
    mov.l   .Lk8e, r1                   /* 8 << 20 */
    bra     .Lnorm
    sub     r1, r2
3:  shll    r8
    rotcl   r5
    rotcl   r4
    bra     .Lnorm
    sub     r0, r2                      /* exponent - 1 */
.Lzero:
    mov     #0, r0
    mov     #0, r1
    rts
    mov.l   @r15+, r8
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
.Lk55:  .long 55 << 20
.Lk1fe: .long 0x001fe000
.Lk8e:  .long 8 << 20
.Lp2:   .long 1, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7, 1 << 8, 1 << 9, 1 << 10, 1 << 11
        .long 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22
        .long 1 << 23, 1 << 24, 1 << 25, 1 << 26, 1 << 27, 1 << 28, 1 << 29, 1 << 30, 1 << 31

/* ---- binary32 add / subtract ---------------------------------------------------------------------------- */
#define FADD N(addsf3)
#define FSUB N(subsf3)
    .align 2
    .global FSUB
FSUB:
    mov.l   .Lf8, r0
    bra     FADD
    xor     r0, r5
.Lfslow:
    mov.l   .Lfc, r0
    jmp     @r0
    nop
.Lfbz:                                  /* b = +-0 (|a| >= |b|): two zeros give -0 only if both are, else a */
    tst     r0, r0
    bf      1f
    mov     r4, r0
    rts
    and     r5, r0
1:  rts
    mov     r4, r0

    .align 2
    .global FADD
FADD:
    mov.l   .Lfm, r3                    /* 0x7fffffff */
    mov     r4, r0
    and     r3, r0                      /* |a| */
    mov     r5, r1
    and     r3, r1                      /* |b| */
    mov.l   .Lfinf, r2                  /* 0x7f800000 */
    cmp/hi  r2, r0
    bt      .Lfslow                     /* NaN */
    cmp/hi  r2, r1
    bt      .Lfslow
    cmp/hs  r1, r0
    bt      1f
    mov     r4, r3                      /* |a| >= |b| */
    mov     r5, r4
    mov     r3, r5
    mov     r0, r3
    mov     r1, r0
    mov     r3, r1
1:  tst     r1, r1
    bt      .Lfbz
    cmp/hs  r2, r0
    bt      .Lfslow                     /* a infinite */
    mov     r1, r6
    and     r2, r6                      /* b's exponent field */
    tst     r6, r6
    bt      .Lfslow                     /* b subnormal */
    mov     r0, r3
    and     r2, r3                      /* a's exponent field */
    mov     r4, r7
    xor     r5, r7
    cmp/pz  r7
    bt      2f
    mov.l   .Lf25, r7                   /* opposite signs: the C path when a's exponent is below 25 */
    cmp/hs  r7, r3
    bf      .Lfslow
2:  sub     r6, r3
    shlr16  r3
    shlr2   r3
    shlr2   r3
    shlr2   r3
    shlr    r3                          /* d */
    mov.l   .Lfse, r7                   /* 0xff800000 */
    and     r4, r7                      /* the result's sign | exponent */
    mov.l   .Lf7f, r2                   /* 0x007fffff */
    and     r2, r0
    and     r2, r1
    mov.l   .Lf80, r2                   /* 0x00800000 */
    or      r2, r0
    or      r2, r1
    shll2   r0
    shll2   r0
    shll2   r0
    mov     r0, r6                      /* ma << 6: the leading 1 at bit 29 */
    shll2   r1
    shll2   r1
    shll2   r1                          /* mb << 6 */
    tst     r3, r3
    bt      .Lfal
    mov     #31, r2
    cmp/hi  r2, r3
    bf      3f
    bra     .Lfal
    mov     #1, r1                      /* d > 31: a sticky bit */
3:  mov     #32, r2
    sub     r3, r2
    shll2   r2
    mova    .Lp2f, r0
    mov.l   @(r0, r2), r0
    dmulu.l r0, r1
    sts     mach, r1                    /* mb >> d */
    sts     macl, r0                    /* the bits shifted out */
    tst     r0, r0
    movt    r0
    xor     #1, r0
    or      r0, r1
.Lfal:
    mov     r4, r0
    xor     r5, r0
    cmp/pz  r0
    bf      .Lfsub
    add     r1, r6
    mov.l   .Lf40, r0                   /* 0x40000000: a carry */
    tst     r0, r6
    bt      .Lfround
    shlr    r6
    movt    r0
    or      r0, r6
    mov.l   .Lf80, r0
    add     r0, r7
    mov.l   .Lfinf, r0                  /* the carry overflowed: +-inf */
    mov     r7, r1
    and     r0, r1
    cmp/eq  r0, r1
    bf      .Lfround
    rts
    mov     r7, r0
.Lfsub:
    sub     r1, r6
    tst     r6, r6
    bt      .Lfzero
.Lfnorm:                                /* the leading 1 back to bit 29 */
    mov.l   .Lf20, r0                   /* 0x20000000 */
    tst     r0, r6
    bf      .Lfround
    mov.l   .Lf3fc, r1                  /* bits 29..22 clear: a byte at once */
    tst     r1, r6
    bf      4f
    shll8   r6
    mov.l   .Lf8e, r1                   /* 8 << 23 */
    bra     .Lfnorm
    sub     r1, r7
4:  shll    r6
    mov.l   .Lf80, r1
    bra     .Lfnorm
    sub     r1, r7
.Lfzero:
    rts
    mov     #0, r0
.Lfround:                               /* 6 bits below: nearest even */
    mov     r6, r0
    and     #0x3f, r0
    shlr2   r6
    shlr2   r6
    shlr2   r6
    mov     #0x20, r1
    cmp/hi  r1, r0
    bt      5f
    cmp/eq  r1, r0
    bf      .Lfpack
    mov     r6, r0
    tst     #1, r0
    bt      .Lfpack
5:  add     #1, r6
    mov.l   .Lf100, r0                  /* 0x01000000: rounded up to 2^24 */
    cmp/eq  r0, r6
    bf      .Lfpack
    shlr    r6
    mov.l   .Lf80, r0
    add     r0, r7
.Lfpack:
    mov.l   .Lfinf, r0                  /* overflow: the exponent field all ones gives +-inf */
    mov     r7, r1
    and     r0, r1
    cmp/eq  r0, r1
    bt      6f
    mov.l   .Lf80, r0
    sub     r0, r7
    mov     r7, r0
    rts
    add     r6, r0
6:  rts
    mov     r7, r0

    .align 2
.Lf8:   .long 0x80000000
.Lfm:   .long 0x7fffffff
.Lfinf: .long 0x7f800000
.Lfse:  .long 0xff800000
.Lf7f:  .long 0x007fffff
.Lf80:  .long 0x00800000
.Lf40:  .long 0x40000000
.Lf20:  .long 0x20000000
.Lf3fc: .long 0x3fc00000
.Lf8e:  .long 8 << 23
.Lf100: .long 0x01000000
.Lf25:  .long 25 << 23
.Lfc:   .long _softfp_addsf3_c
.Lp2f:  .long 1, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7, 1 << 8, 1 << 9, 1 << 10, 1 << 11
        .long 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22
        .long 1 << 23, 1 << 24, 1 << 25, 1 << 26, 1 << 27, 1 << 28, 1 << 29, 1 << 30, 1 << 31

/* ---- binary64 compares: fp-bit's __fpcmp_parts values ------------------------------------------------------ */
    .align 2
    .global N(eqdf2), N(nedf2), N(ltdf2), N(ledf2), N(gtdf2), N(gedf2)
N(gtdf2):
N(gedf2):
    bra     .Lcmp
    mov     #-1, r3                     /* the answer with a NaN */
N(eqdf2):
N(nedf2):
N(ltdf2):
N(ledf2):
    mov     #1, r3
.Lcmp:
    mov.l   .Lc7ff, r2                  /* 0x7ff00000 */
    mov     r4, r0
    and     r2, r0
    cmp/eq  r2, r0
    bt      .Lcna                       /* a: inf or NaN */
.Lcb:
    mov     r6, r0
    and     r2, r0
    cmp/eq  r2, r0
    bt      .Lcnb
.Lcok:
    mov     r4, r0
    xor     r6, r0
    cmp/pz  r0
    bf      .Lcdiff
    cmp/eq  r6, r4                      /* the same sign: the bit patterns' order (reversed when negative) */
    bf      1f
    cmp/eq  r7, r5
    bt      .Lceq
    bra     2f
    cmp/hi  r7, r5
1:  cmp/hi  r6, r4
2:  movt    r0                          /* |a| > |b| */
    cmp/pz  r4
    bt      3f
    xor     #1, r0
3:  add     r0, r0
    rts
    add     #-1, r0
.Lceq:
    rts
    mov     #0, r0
.Lcdiff:                                /* opposite signs: equal only as +0 and -0 */
    mov     r4, r0
    or      r6, r0
    add     r0, r0
    or      r5, r0
    or      r7, r0
    tst     r0, r0
    bt      .Lceq
    cmp/pz  r4
    mov     #1, r0
    bt      4f
    mov     #-1, r0
4:  rts
    nop
.Lcna:                                  /* a's exponent all ones: NaN if its fraction is not 0 */
    mov.l   .Lcfr, r1
    mov     r4, r0
    and     r1, r0
    or      r5, r0
    tst     r0, r0
    bt      .Lcb
    rts
    mov     r3, r0
.Lcnb:
    mov.l   .Lcfr, r1
    mov     r6, r0
    and     r1, r0
    or      r7, r0
    tst     r0, r0
    bt      .Lcok
    rts
    mov     r3, r0

    .align 2
.Lc7ff: .long 0x7ff00000
.Lcfr:  .long 0x000fffff

/* ---- binary64 to binary32 ----------------------------------------------------------------------------------- */
/* a normal double whose float is normal (exponent field 897 .. 1150): the exponent rebiased in place (- 896 << 20:
   1 .. 254 in bits 20..27), the 64-bit word shifted left 3 places (the sign falls out and is put back; the float's
   exponent and 23 fraction bits in the high word, the 29 bits below them at the top of the low word), round to
   nearest even on those (a carry out of the fraction raises the exponent, to +-inf from 254 at most); everything
   else (zeros, subnormal results, overflow, infinities, NaNs) to softfp.c's C, softfp_truncdfsf2_c */
#define TRUNC N(truncdfsf2)
    .align 2
    .global TRUNC
TRUNC:
    mov.l   .Ltk7ff, r1                 /* 0x7ff00000 */
    mov     r4, r0
    and     r1, r0
    mov.l   .Ltklo, r1                  /* 897 << 20 */
    sub     r1, r0
    mov.l   .Ltkrg, r1                  /* (1150 - 897) << 20 */
    cmp/hi  r1, r0
    bt      .Ltslow
    mov.l   .Ltkb, r1                   /* 896 << 20 */
    mov     r4, r0
    sub     r1, r0                      /* sign | (1 .. 254) << 20 | fraction */
    mov.l   .Ltk8, r1                   /* 0x80000000 */
    mov     r0, r2
    and     r1, r2                      /* the sign */
    shll    r5
    rotcl   r0
    shll    r5
    rotcl   r0
    shll    r5
    rotcl   r0
    or      r2, r0                      /* truncated */
    cmp/hi  r1, r5                      /* more than half */
    bt      1f
    cmp/eq  r1, r5                      /* a tie: to even */
    bf      2f
    tst     #1, r0
    bt      2f
1:  add     #1, r0
2:  rts
    nop
.Ltslow:
    mov.l   .Ltc, r0
    jmp     @r0
    nop

    .align 2
.Ltk7ff: .long 0x7ff00000
.Ltklo: .long 897 << 20
.Ltkrg: .long (1150 - 897) << 20
.Ltkb:  .long 896 << 20
.Ltk8:  .long 0x80000000
.Ltc:   .long _softfp_truncdfsf2_c

/* ---- a debris piece's y move (pobj.c rubblepiece_step): y + yVel rounded to binary32, then yVel += yAcc ------- */
/* PIECEY(y, v): v points at yVel, yAcc 16 bytes on (struct pin_ext's num xVel, yVel, xAcc, yAcc). Returns the bits
   of __truncdfsf2 (__adddf3 (__extendsfdf2 (y), v[0])) (y widened here when normal, else by __extendsfdf2; then ADD
   and TRUNC); stores v[0] = __adddf3 (v[0], v[2]) through a one-entry memo of the last operands' and result's bits
   (a hit is ADD's result for the same operands; the zeroed memo holds +0 + +0 = +0): the pieces of one explosion
   or drain move with the same velocities */
#ifdef SOFTFP_SFNAMES
#define PIECEY _sf_piece_y
#else
#define PIECEY _softfp_piece_y
#endif
    .align 2
    .global PIECEY
PIECEY:
    sts.l   pr, @-r15
    mov.l   r9, @-r15
    mov.l   r10, @-r15
    mov     r5, r9                      /* v */
    mov.l   .Lpfinf, r1                 /* 0x7f800000 */
    mov     r4, r0
    and     r1, r0
    tst     r0, r0
    bt      .Lpwide                     /* zero, subnormal */
    cmp/eq  r1, r0
    bt      .Lpwide                     /* infinite, NaN */
    mov     r4, r5
    shll16  r5
    shll8   r5
    shll2   r5
    shll2   r5
    shll    r5                          /* the low word: the fraction's 3 low bits at the top */
    mov.l   .Lpk8, r1                   /* 0x80000000 */
    mov     r4, r2
    and     r1, r2                      /* the sign */
    mov.l   .Lpfm, r1                   /* 0x7fffffff */
    and     r1, r4
    shlr2   r4
    shlr    r4                          /* exponent and fraction 3 places right */
    mov.l   .Lpkb, r1                   /* 896 << 20: the exponent rebiased */
    add     r1, r4
    or      r2, r4                      /* the high word */
.Lpadd:
    mov.l   @r9, r6
    bsr     ADD
    mov.l   @(4, r9), r7
    mov.l   .Lp7ff, r3                  /* TRUNC's fast path (its comment) */
    mov     r0, r2
    and     r3, r2
    mov.l   .Lpklo, r3
    sub     r3, r2
    mov.l   .Lpkrg, r3
    cmp/hi  r3, r2
    bt      .Lptrunc
    mov.l   .Lpkb, r3
    sub     r3, r0
    mov.l   .Lpk8, r3
    mov     r0, r2
    and     r3, r2
    shll    r1
    rotcl   r0
    shll    r1
    rotcl   r0
    shll    r1
    rotcl   r0
    or      r2, r0
    cmp/hi  r3, r1
    bt      1f
    cmp/eq  r3, r1
    bf      2f
    tst     #1, r0
    bt      2f
1:  add     #1, r0
2:  mov     r0, r10                     /* the new y */
    mov.l   @r9, r4
    mov.l   @(4, r9), r5
    mov.l   @(16, r9), r6
    mov.l   @(20, r9), r7
    mov.l   .Lpmemo, r1
    mov.l   @r1, r0
    cmp/eq  r0, r4
    bf      .Lpmiss
    mov.l   @(4, r1), r0
    cmp/eq  r0, r5
    bf      .Lpmiss
    mov.l   @(8, r1), r0
    cmp/eq  r0, r6
    bf      .Lpmiss
    mov.l   @(12, r1), r0
    cmp/eq  r0, r7
    bf      .Lpmiss
    mov.l   @(16, r1), r0
    mov.l   @(20, r1), r1
.Lpput:
    mov.l   r0, @r9
    mov.l   r1, @(4, r9)
    mov     r10, r0
    mov.l   @r15+, r10
    mov.l   @r15+, r9
    lds.l   @r15+, pr
    rts
    nop
.Lpmiss:
    mov.l   r4, @r1
    mov.l   r5, @(4, r1)
    mov.l   r6, @(8, r1)
    bsr     ADD
    mov.l   r7, @(12, r1)
    mov.l   .Lpmemo, r2
    mov.l   r0, @(16, r2)
    bra     .Lpput
    mov.l   r1, @(20, r2)
.Lpwide:
    mov.l   .Lpext, r0
    jsr     @r0
    nop
    mov     r0, r4
    bra     .Lpadd
    mov     r1, r5
.Lptrunc:                               /* not a normal float: TRUNC (its C path) */
    mov     r0, r4
    bsr     TRUNC
    mov     r1, r5
    bra     2b
    nop

    .align 2
.Lpfinf: .long 0x7f800000
.Lpk8:  .long 0x80000000
.Lpfm:  .long 0x7fffffff
.Lpkb:  .long 896 << 20
.Lp7ff: .long 0x7ff00000
.Lpklo: .long 897 << 20
.Lpkrg: .long (1150 - 897) << 20
.Lpmemo: .long .Lpm
.Lpext: .long N(extendsfdf2)

    .bss
    .align 2
.Lpm:   .space 24                       /* yVel, yAcc, yVel + yAcc (high word first) */
