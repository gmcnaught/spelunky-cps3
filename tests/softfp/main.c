/* tests/softfp on the SH-2: src/sh2/softfp.c (built with SOFTFP_SFNAMES: sf_* names) against libgcc's fp-bit
   (__adddf3 ...) on the cases of cases.h, the same sequence as the host test. Per operation: the cases, the cases
   whose raw result differs from fp-bit's (bits, NaN payloads and compare values included), the first four of
   them, the hash of the canonical results (check.h; equal to the host test's hash = equal to the host's FPU),
   and the clocks of 256 calls of each on game-like operands. Results in sprite RAM (unused: no video), read by
   scripts/lua/softfp.lua when the state word is 1:
     0x04000000  'SFPT', state, operations, cases per operation
     0x04000100  per operation 16 words: cases, differ, hash, fp-bit clocks / 256 calls, softfp clocks / 256
                 calls, loop clocks / 256, then 4 x (a hi, a lo) ... see rec_put
   CASES (make): cases per operation. */
#include <stdint.h>
#include "check.h"

#ifndef CASES
#define CASES 500000
#endif

#define R32(a) (*(volatile uint32_t *)(a))

/* fp-bit (libgcc) and softfp.c (sf_*) */
#define DECL(n, rt, ...) rt __##n(__VA_ARGS__); rt sf_##n(__VA_ARGS__);
DECL(adddf3, uint64_t, uint64_t, uint64_t) DECL(subdf3, uint64_t, uint64_t, uint64_t)
DECL(muldf3, uint64_t, uint64_t, uint64_t) DECL(divdf3, uint64_t, uint64_t, uint64_t)
DECL(eqdf2, int, uint64_t, uint64_t) DECL(nedf2, int, uint64_t, uint64_t) DECL(ltdf2, int, uint64_t, uint64_t)
DECL(ledf2, int, uint64_t, uint64_t) DECL(gtdf2, int, uint64_t, uint64_t) DECL(gedf2, int, uint64_t, uint64_t)
DECL(unorddf2, int, uint64_t, uint64_t)
DECL(extendsfdf2, uint64_t, uint32_t) DECL(truncdfsf2, uint32_t, uint64_t) DECL(floatsidf, uint64_t, int32_t)
DECL(floatunsidf, uint64_t, uint32_t) DECL(fixdfsi, int32_t, uint64_t)
DECL(addsf3, uint32_t, uint32_t, uint32_t) DECL(subsf3, uint32_t, uint32_t, uint32_t)
DECL(mulsf3, uint32_t, uint32_t, uint32_t) DECL(divsf3, uint32_t, uint32_t, uint32_t)
DECL(eqsf2, int, uint32_t, uint32_t) DECL(nesf2, int, uint32_t, uint32_t) DECL(ltsf2, int, uint32_t, uint32_t)
DECL(lesf2, int, uint32_t, uint32_t) DECL(gtsf2, int, uint32_t, uint32_t) DECL(gesf2, int, uint32_t, uint32_t)
DECL(floatsisf, uint32_t, int32_t) DECL(fixsfsi, int32_t, uint32_t)
uint32_t sf_piece_y(uint32_t y, uint64_t *v);      /* softfp_sh2.S's debris y move (v[0] yVel, v[2] yAcc) */


/* the piece's y move: fp-bit's calls (impl 0), softfp_sh2.S's (1); the new y, and yVel + yAcc in *nv */
static uint32_t piece(int impl, uint32_t y, uint64_t v, uint64_t acc, uint64_t *nv)
{
    uint64_t w[3];
    uint32_t r;
    if (!impl) {
        *nv = __adddf3(v, acc);
        return __truncdfsf2(__adddf3(__extendsfdf2(y), v));
    }
    w[0] = v; w[1] = 0; w[2] = acc;
    r = sf_piece_y(y, w);
    *nv = w[0];
    return r;
}

/* raw result of op through fp-bit (impl 0) or softfp (impl 1); compares as their int, sign-extended */
static uint64_t run(int impl, int op, uint64_t a, uint64_t b)
{
    uint32_t fa = (uint32_t)a, fb = (uint32_t)b;
#define P(n, ...) (impl ? sf_##n(__VA_ARGS__) : __##n(__VA_ARGS__))
    switch (op) {
    case OP_ADD: return P(adddf3, a, b);
    case OP_SUB: return P(subdf3, a, b);
    case OP_MUL: return P(muldf3, a, b);
    case OP_DIV: return P(divdf3, a, b);
    case OP_EQ: return (uint64_t)(int64_t)P(eqdf2, a, b);
    case OP_NE: return (uint64_t)(int64_t)P(nedf2, a, b);
    case OP_LT: return (uint64_t)(int64_t)P(ltdf2, a, b);
    case OP_LE: return (uint64_t)(int64_t)P(ledf2, a, b);
    case OP_GT: return (uint64_t)(int64_t)P(gtdf2, a, b);
    case OP_GE: return (uint64_t)(int64_t)P(gedf2, a, b);
    case OP_UNORD: return (uint64_t)(int64_t)P(unorddf2, a, b);
    case OP_EXT: return P(extendsfdf2, fa);
    case OP_TRUNC: case OP_TRUNCE: return P(truncdfsf2, a);
    case OP_I2D: return P(floatsidf, (int32_t)fa);
    case OP_U2D: return P(floatunsidf, fa);
    case OP_D2I: return (uint32_t)P(fixdfsi, a);
    case OP_FADD: return P(addsf3, fa, fb);
    case OP_FSUB: return P(subsf3, fa, fb);
    case OP_FMUL: return P(mulsf3, fa, fb);
    case OP_FDIV: return P(divsf3, fa, fb);
    case OP_FEQ: return (uint64_t)(int64_t)P(eqsf2, fa, fb);
    case OP_FNE: return (uint64_t)(int64_t)P(nesf2, fa, fb);
    case OP_FLT: return (uint64_t)(int64_t)P(ltsf2, fa, fb);
    case OP_FLE: return (uint64_t)(int64_t)P(lesf2, fa, fb);
    case OP_FGT: return (uint64_t)(int64_t)P(gtsf2, fa, fb);
    case OP_FGE: return (uint64_t)(int64_t)P(gesf2, fa, fb);
    case OP_I2F: return P(floatsisf, (int32_t)fa);
    case OP_F2I: return (uint32_t)P(fixsfsi, fa);
    }
    return 0;
#undef P
}

/* the canonical form of a raw softfp result (check.h): what the host test hashes */
static uint64_t canon(int op, uint64_t r)
{
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_EXT: return canon_d(r);
    case OP_TRUNC: case OP_TRUNCE: case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV: return canon_f((uint32_t)r);
    case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE: case OP_FEQ: case OP_FNE: case OP_FLT:
    case OP_FLE: case OP_FGT: case OP_FGE: return rel(op, (int)(int64_t)r);
    case OP_UNORD: return r != 0;
    default: return r;
    }
}

static uint32_t frc(void)
{
    volatile uint8_t *f = (volatile uint8_t *)0xfffffe12u;
    uint32_t h = f[0], l = f[1], h2 = f[0];
    if (h2 != h) { l = f[1]; h = h2; }
    return h << 8 | l;
}

/* game-like operands for the timing: positions, velocities, scales (normal values, as the play loop's) */
static const uint64_t tv[8] = { 0x406b000000000000ull, 0x3ff0000000000000ull, 0xc00b99999999999aull,
    0x3fc3333333333333ull, 0x407f100000000000ull, 0x4030000000000000ull, 0x3fd3333333333333ull,
    0xbff0000000000000ull };
static const uint32_t tf[8] = { 0x43580000u, 0x43b00000u, 0x3ecccccdu, 0x3f800000u, 0xc0400000u, 0x3e19999au,
    0x41800000u, 0xbf800000u };
static volatile uint64_t sink;

/* clocks of 256 direct calls of op's fp-bit (impl 0) or softfp (impl 1) entry on game-like operands, a loop
   without the call subtracted (the case loop's dispatch through run() is not timed) */
#define LOOPD(fn, T) do { for (k = 0; k < 256; k++) sink = (uint64_t)fn(tv[k & 7], tv[(k + 3) & 7]); } while (0)
#define LOOPF(fn) do { for (k = 0; k < 256; k++) sink = (uint64_t)fn(tf[k & 7], tf[(k + 3) & 7]); } while (0)
#define LOOP1(fn, arr) do { for (k = 0; k < 256; k++) sink = (uint64_t)fn(arr[k & 7]); } while (0)
static const int32_t ti[8] = { 216, -3, 497, 16, 0, -1000, 70000, 5 };
static uint32_t time256(int impl, int op)
{
    uint32_t t0 = frc(), k;
    switch (op) {
#define D2(o, f) case o: if (impl) LOOPD(sf_##f, 0); else LOOPD(__##f, 0); break;
#define F2(o, f) case o: if (impl) LOOPF(sf_##f); else LOOPF(__##f); break;
#define U1(o, f, arr) case o: if (impl) LOOP1(sf_##f, arr); else LOOP1(__##f, arr); break;
    D2(OP_ADD, adddf3) D2(OP_SUB, subdf3) D2(OP_MUL, muldf3) D2(OP_DIV, divdf3) D2(OP_EQ, eqdf2) D2(OP_NE, nedf2)
    D2(OP_LT, ltdf2) D2(OP_LE, ledf2) D2(OP_GT, gtdf2) D2(OP_GE, gedf2) D2(OP_UNORD, unorddf2)
    U1(OP_EXT, extendsfdf2, tf) U1(OP_TRUNC, truncdfsf2, tv) U1(OP_I2D, floatsidf, ti)
    U1(OP_U2D, floatunsidf, ((const uint32_t *)ti)) U1(OP_D2I, fixdfsi, tv)
    F2(OP_FADD, addsf3) F2(OP_FSUB, subsf3) F2(OP_FMUL, mulsf3) F2(OP_FDIV, divsf3) F2(OP_FEQ, eqsf2)
    F2(OP_FNE, nesf2) F2(OP_FLT, ltsf2) F2(OP_FLE, lesf2) F2(OP_FGT, gtsf2) F2(OP_FGE, gesf2)
    U1(OP_I2F, floatsisf, ti) U1(OP_F2I, fixsfsi, tf)
    case OP_PIECE: case OP_PIECEE: case OP_PIECEH: { /* yVel + yAcc repeated in 4 calls of 4 */
        uint64_t nv;
        for (k = 0; k < 256; k++) sink = piece(impl, tf[k & 7], tv[(k >> 2) & 7], 0x3fe3333333333333ull, &nv);
        break;
    }
    default:                                         /* impl 2: the loop alone */
        for (k = 0; k < 256; k++) sink = tv[k & 7] ^ tv[(k + 3) & 7];
    }
    return ((frc() - t0) & 0xffff) * 8;
}

/* OPMASK (make): the operations run (bit op; default all) */
#ifndef OPMASK
#define OPMASK 0xffffffffffffffffull
#endif

int main(void)
{
    int op;
    R32(0x04000004) = 0;
    R32(0x04000008) = OP_N;
    R32(0x0400000c) = CASES;
    R32(0x04000000) = 0x53465054;                  /* 'SFPT' */
    for (op = 0; op < OP_N; op++) {
        volatile uint32_t *o = (volatile uint32_t *)(0x04000100 + 64 * op);
        uint32_t h = 2166136261u, nb = 0;
        long k, n = ((uint64_t)(OPMASK) >> op) & 1 ? cs_count(op, CASES) : 0;
        int i;
        cs_state = 0x9e3779b97f4a7c15ull + (uint64_t)op;
        for (k = 0; k < n; k++) {
            struct cs_case c;
            uint64_t f, s, v, fv = 0, sv = 0;
            cs_k = k;
            c = cs_case_for(op);
            if (cs_piece(op)) {                     /* the new y and yVel + yAcc both */
                f = piece(0, (uint32_t)c.a, c.b, c.c, &fv);
                s = piece(1, (uint32_t)c.a, c.b, c.c, &sv);
                if (fv != sv) f = ~s;
                v = canon_f((uint32_t)s);
                for (i = 0; i < 64; i += 8) { h ^= (uint32_t)(canon_d(sv) >> i) & 0xff; h *= 16777619u; }
            } else {
                f = run(0, op, c.a, c.b); s = run(1, op, c.a, c.b); v = canon(op, s);
            }
            if (f != s) {
                if (nb < 2) {
                    o[6 + 5 * nb] = (uint32_t)(c.a >> 32);
                    o[7 + 5 * nb] = (uint32_t)c.a;
                    o[8 + 5 * nb] = (uint32_t)c.b;      /* b's low word (its high word: o[9]) */
                    o[9 + 5 * nb] = (uint32_t)(c.b >> 32);
                    o[10 + 5 * nb] = (uint32_t)(f ^ s);
                }
                nb++;
            }
            for (i = 0; i < 64; i += 8) { h ^= (uint32_t)(v >> i) & 0xff; h *= 16777619u; }
        }
        o[0] = (uint32_t)n;
        o[1] = nb;
        o[2] = h;
        o[3] = n ? time256(0, op) : 0;
        o[4] = n ? time256(1, op) : 0;
        o[5] = n ? time256(0, -1) : 0;
        R32(0x04000010) = (uint32_t)op + 1;         /* progress */
    }
    R32(0x04000004) = 1;
    for (;;) ;
}
