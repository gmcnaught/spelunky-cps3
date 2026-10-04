/* tests/softfp: one case of an operation through a soft-float implementation (the SF_* macros name it), and the
   canonical form of its result that the host's FPU must give too: arithmetic and conversion results as bits with
   every NaN as the default NaN; compares as the truth of the relation GCC tests (eq: == 0, lt: < 0, ...). The
   host (host.c) and the SH-2 (main.c) both hash these per operation; equal hashes = equal to the host's FPU. */
#ifndef SOFTFP_CHECK_H
#define SOFTFP_CHECK_H
#include "cases.h"

struct cs_case { uint64_t a, b; };
/* (3% of the double pairs are carry / sticky shapes, 3% long cancellations one binade apart; floats too) */

static inline uint64_t canon_d(uint64_t r) { return ((r & 0x7fffffffffffffffull) > 0x7ff0000000000000ull) ? 0x7ff8000000000000ull : r; }
static inline uint32_t canon_f(uint32_t r) { return ((r & 0x7fffffffu) > 0x7f800000u) ? 0x7fc00000u : r; }

/* the operands of case k of op (drawn in order: call once per case) */
static inline struct cs_case cs_case_for(int op)
{
    struct cs_case c;
    switch (op) {
    case OP_EXT: case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV: case OP_FEQ: case OP_FNE: case OP_FLT:
    case OP_FLE: case OP_FGT: case OP_FGE: case OP_F2I:
        if (cs_u32(100) < 3) {                       /* the same shape in binary32 */
            uint32_t ex = 2 + cs_u32(250), s = cs_u32(2) << 31;
            c.a = s | (ex << 23) | ((uint32_t)cs_next() & ((1u << cs_u32(23)) - 1));
            c.b = (s ^ 0x80000000u) | ((ex - 1) << 23) |
                  ((0x7fffffu - ((uint32_t)cs_next() & ((1u << cs_u32(23)) - 1))) | cs_u32(2));
            break;
        }
        c.a = cs_f1();
        c.b = cs_f2((uint32_t)c.a);
        break;
    case OP_I2D: case OP_U2D: case OP_I2F:
        c.a = (uint32_t)cs_i32();
        c.b = 0;
        break;
    default:
        if (cs_u32(100) < 3) {                       /* a carry with a sticky bit: a's significand near all ones, b
                                                        32..63 binades below with a few low bits */
            uint64_t ex = 1 + cs_u32(2000), s = (uint64_t)cs_u32(2) << 63;
            c.a = s | (ex << 52) | (0x000fffffffffffffull - cs_u32(4));
            uint32_t d = 32 + cs_u32(32), lo = cs_u32(32);
            c.b = s | ((ex > d ? ex - d : 1) << 52) | (uint64_t)(cs_next() & ((1ull << lo) - 1)) | ((uint64_t)cs_u32(4) << 50);
            if (cs_u32(4) == 0) c.b ^= 0x8000000000000000ull;
            break;
        }
        if (cs_u32(100) < 3) {                       /* a long cancellation one binade apart: a = 2^e (1 + small),
                                                        b = -2^(e - 1) (2 - small) with its last bits set */
            uint64_t ex = 2 + cs_u32(2000), s = (uint64_t)cs_u32(2) << 63;
            c.a = s | (ex << 52) | (cs_next() & ((1ull << cs_u32(52)) - 1));
            c.b = (s ^ 0x8000000000000000ull) | ((ex - 1) << 52) |
                  ((0x000fffffffffffffull - (cs_next() & ((1ull << cs_u32(52)) - 1))) | cs_u32(2));
            break;
        }
        c.a = cs_d1();
        c.b = cs_d2(c.a);
        break;
    }
    return c;
}

/* compare results: the relation GCC derives from the return value */
static inline uint64_t rel(int op, int r)
{
    switch (op) {
    case OP_EQ: case OP_FEQ: return r == 0;
    case OP_NE: case OP_FNE: return r != 0;
    case OP_LT: case OP_FLT: return r < 0;
    case OP_LE: case OP_FLE: return r <= 0;
    case OP_GT: case OP_FGT: return r > 0;
    case OP_GE: case OP_FGE: return r >= 0;
    default: return r != 0;
    }
}
#endif
