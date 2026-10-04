/* tests/gametime: the FRT output-compare interrupt (vector 72, hooks.c frt_start): the FRC's wraps counted for
   hooks.c now() (an FRC below its value at the previous interrupt: one wrap; the interrupt comes every 4,096 ticks),
   OCRA moved on by 4,096 ticks */
    .text
    .align 2
    .global _frt_isr
_frt_isr:
    mov.l   r0, @-r15
    mov.l   r1, @-r15
    mov.l   r2, @-r15
    mov.l   r3, @-r15
    mov.l   k_ftcsr, r1
    mov.b   @r1, r0
    and     #0xf7, r0                   /* OCFA <- 0 */
    mov.b   r0, @r1
    mov.l   k_frc, r1                   /* FRC: high, low, high again (and low again if the high byte moved) */
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
    mov.l   k_lastf, r1
    mov.l   @r1, r0
    cmp/hs  r0, r2
    bt      2f
    mov.l   k_exth, r3
    mov.l   @r3, r0
    add     #1, r0
    mov.l   r0, @r3
2:  mov.l   r2, @r1
    mov.w   k_per, r0
    add     r0, r2
    mov.l   k_ocra, r1
    mov     r2, r0
    shlr8   r0
    mov.b   r0, @r1
    mov     r2, r0
    mov.b   r0, @(1, r1)
    mov.l   @r15+, r3
    mov.l   @r15+, r2
    mov.l   @r15+, r1
    mov.l   @r15+, r0
    rte
    nop
    .align 1
k_per:   .word 4096
    .align 2
k_ftcsr: .long 0xfffffe11
k_frc:   .long 0xfffffe12
k_ocra:  .long 0xfffffe14
k_exth:  .long _frt_exth
k_lastf: .long _frt_lastf
