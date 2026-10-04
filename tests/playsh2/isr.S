/* tests/playsh2 interrupt handlers (vector table at VBR = _vtab, main.c): the SH7604 FRT
     vector 73  FOVI: the FRC overflowed: _ovf_n counted, OVF cleared (not enabled: main.c uses the wrap count below)
     vector 72  FOCI (output compare A), every period ticks (all builds; 4,096 unless PROF): counts the FRC's
                wraps for main.c now() (MAME raises the overflow interrupt at its own timeslice, up to a wrap early
                or late, so the overflow count is not used); PROF builds: while the tag (the work's kind) equals the
                wanted kind, the interrupted PC and PR logged as 16-byte buckets from 0x06000000 (one word each);
                then OCRA moved on by the period
   the counters and the log live in character RAM (main.c P_*): main.c clears main RAM between jobs. */
/* PBASE: the header block (main.c P_*): character RAM in the MAME builds, main RAM (.trace) in JT builds */
#ifdef JT
#define PBASE 0x02000000
#else
#define PBASE 0x04100000
#endif
    .section .text
    .align 2
    .global _ovf_isr
_ovf_isr:
    mov.l   r0, @-r15
    mov.l   r1, @-r15
    mov.l   k_ftcsr, r1
    mov.b   @r1, r0
    and     #0xfd, r0           /* OVF (bit 1) <- 0 */
    mov.b   r0, @r1
    mov.l   k_ovfn, r1
    mov.l   @r1, r0
    add     #1, r0
    mov.l   r0, @r1
    mov.l   @r15+, r1
    mov.l   @r15+, r0
    rte
    nop

    .global _prof_isr
_prof_isr:
    mov.l   r0, @-r15
    mov.l   r1, @-r15
    mov.l   r2, @-r15
    mov.l   r3, @-r15
    mov.l   k_ftcsr, r1
    mov.b   @r1, r0
    and     #0xf7, r0           /* OCFA (bit 3) <- 0 */
    mov.b   r0, @r1
    /* FRC: high, low, high again (and low again if the high byte moved: MAME has no latch) */
    mov.l   k_frc, r1
    mov.b   @r1, r2
    mov.b   @(1, r1), r0
    mov.b   @r1, r3
    cmp/eq  r2, r3
    bt      1f
    mov.b   @(1, r1), r0
    mov     r3, r2
1:  extu.b  r2, r2
    shll8   r2
    extu.b  r0, r0
    or      r0, r2
    /* the wrap count (main.c now()): FRC below its value at the previous interrupt = one wrap (the interrupt
       comes every period < 65,536 ticks) */
    mov.l   k_lastf, r1
    mov.l   @r1, r0
    cmp/hs  r0, r2
    bt      2f
    mov.l   k_exth, r3
    mov.l   @r3, r0
    add     #1, r0
    mov.l   r0, @r3
2:  mov.l   r2, @r1
    /* OCRA = FRC + period (OCRA high then low) */
    mov.l   k_per, r1
    mov.l   @r1, r0
    add     r0, r2
    mov.l   k_ocra, r1
    mov     r2, r0
    shlr8   r0
    mov.b   r0, @r1
    mov     r2, r0
    mov.b   r0, @(1, r1)
    /* log while the tag equals _prof_want: ((PC - 0x06000000) >> 4) << 16 | (PR - 0x06000000) >> 4, one word */
    mov.l   k_tag, r1
    mov.l   @r1, r3
    mov.l   k_want, r1
    mov.l   @r1, r0
    cmp/eq  r0, r3
    bf      9f
    mov.l   k_skc, r1           /* only every _prof_skip-th interrupt logs */
    mov.l   @r1, r0
    add     #1, r0
    mov.l   k_skip, r2
    mov.l   @r2, r2
    cmp/hs  r2, r0
    bf/s    9f
    mov.l   r0, @r1
    mov     #0, r0
    mov.l   r0, @r1
    mov.l   k_pn, r1
    mov.l   @r1, r2
    mov.l   k_pmax, r0
    cmp/hs  r0, r2
    bt      9f
    add     #1, r2
    mov.l   r2, @r1
    add     #-1, r2
    shll2   r2
    mov.l   k_plog, r0
    add     r0, r2
    mov.l   k_base, r1
    mov.l   @(16, r15), r0      /* the interrupted PC (4 registers pushed) */
    sub     r1, r0
    shlr2   r0
    shlr2   r0
    shll16  r0
    mov.l   k_wrapf, r3         /* PROF_WRAP builds: the last soft-float caller (wrap.S) in place of PR */
    mov.l   @r3, r3
    tst     r3, r3
    bt/s    1f
    sts     pr, r3
    mov.l   k_lastc, r3
    mov.l   @r3, r3
1:  sub     r1, r3
    shlr2   r3
    shlr2   r3
    extu.w  r3, r3
    or      r3, r0
    mov.l   r0, @r2
9:  mov.l   @r15+, r3
    mov.l   @r15+, r2
    mov.l   @r15+, r1
    mov.l   @r15+, r0
    rte
    nop

    .align 2
k_ftcsr: .long 0xfffffe11
k_frc:   .long 0xfffffe12
k_ocra:  .long 0xfffffe14
k_ovfn:  .long PBASE + 0x10
k_tag:   .long PBASE + 0x14
k_pn:    .long PBASE + 0x18
k_per:   .long PBASE + 0x1c
k_pmax:  .long 65536
k_want:  .long PBASE + 0x20
k_base:  .long 0x06000000
k_skc:   .long PBASE + 0x24
k_skip:  .long PBASE + 0x28
k_lastc: .long PBASE + 0x2c
k_wrapf: .long PBASE + 0x30
k_exth:  .long PBASE + 0x34
k_lastf: .long PBASE + 0x38
k_plog:  .long 0x04140000
