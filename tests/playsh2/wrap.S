/* tests/playsh2 PROF_WRAP builds (Makefile): each libgcc soft-float entry the code calls goes through a wrapper
   that stores its caller's return address (PR) at 0x0410002c, then jumps to the real routine (PR kept: it
   returns to the caller). isr.S logs that address in place of PR, so the samples inside soft-float are
   attributed to the function that called it. The list: SFWRAP in the Makefile (the helpers sh-elf-nm finds in main.elf). */
    .section .text
    .align 2
    .global ___wrap___adddf3
___wrap___adddf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___adddf3
    .align 2
    .global ___wrap___subdf3
___wrap___subdf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___subdf3
    .align 2
    .global ___wrap___muldf3
___wrap___muldf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___muldf3
    .align 2
    .global ___wrap___divdf3
___wrap___divdf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___divdf3
    .align 2
    .global ___wrap___eqdf2
___wrap___eqdf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___eqdf2
    .align 2
    .global ___wrap___nedf2
___wrap___nedf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___nedf2
    .align 2
    .global ___wrap___gtdf2
___wrap___gtdf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___gtdf2
    .align 2
    .global ___wrap___gedf2
___wrap___gedf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___gedf2
    .align 2
    .global ___wrap___ltdf2
___wrap___ltdf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___ltdf2
    .align 2
    .global ___wrap___ledf2
___wrap___ledf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___ledf2
    .align 2
    .global ___wrap___extendsfdf2
___wrap___extendsfdf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___extendsfdf2
    .align 2
    .global ___wrap___truncdfsf2
___wrap___truncdfsf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___truncdfsf2
    .align 2
    .global ___wrap___fixdfsi
___wrap___fixdfsi:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___fixdfsi
    .align 2
    .global ___wrap___floatsidf
___wrap___floatsidf:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___floatsidf
    .align 2
    .global ___wrap___floatunsidf
___wrap___floatunsidf:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___floatunsidf
    .align 2
    .global ___wrap___addsf3
___wrap___addsf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___addsf3
    .align 2
    .global ___wrap___subsf3
___wrap___subsf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___subsf3
    .align 2
    .global ___wrap___mulsf3
___wrap___mulsf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___mulsf3
    .align 2
    .global ___wrap___divsf3
___wrap___divsf3:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___divsf3
    .align 2
    .global ___wrap___gtsf2
___wrap___gtsf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___gtsf2
    .align 2
    .global ___wrap___gesf2
___wrap___gesf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___gesf2
    .align 2
    .global ___wrap___ltsf2
___wrap___ltsf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___ltsf2
    .align 2
    .global ___wrap___lesf2
___wrap___lesf2:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___lesf2
    .align 2
    .global ___wrap___fixsfsi
___wrap___fixsfsi:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___fixsfsi
    .align 2
    .global ___wrap___floatsisf
___wrap___floatsisf:
    mov.l   1f, r1
    sts     pr, r2
    mov.l   r2, @r1
    mov.l   2f, r1
    jmp     @r1
    nop
    .align 2
1:  .long   0x0410002c
2:  .long   ___real___floatsisf
