/* tests/softfp host test: src/sh2/softfp.c (built with SOFTFP_HOST: sf_* names) against the host's FPU on the
   cases of cases.h. Per operation: the number of cases whose canonical result (check.h) differs from the FPU's,
   the first few listed, and the hash of the canonical results (the SH-2 test prints the same hash for its own
   results: equal = equal to this FPU). Also the fp-bit rules the FPU does not show: NaN compare values, (int)
   saturation.
     build/host [cases per operation]          (cc -O2 -ffp-contract=off) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"

uint64_t sf_adddf3(uint64_t, uint64_t); uint64_t sf_subdf3(uint64_t, uint64_t); uint64_t sf_muldf3(uint64_t, uint64_t);
uint64_t sf_divdf3(uint64_t, uint64_t);
int sf_eqdf2(uint64_t, uint64_t); int sf_nedf2(uint64_t, uint64_t); int sf_ltdf2(uint64_t, uint64_t);
int sf_ledf2(uint64_t, uint64_t); int sf_gtdf2(uint64_t, uint64_t); int sf_gedf2(uint64_t, uint64_t);
int sf_unorddf2(uint64_t, uint64_t);
uint64_t sf_extendsfdf2(uint32_t); uint32_t sf_truncdfsf2(uint64_t); uint64_t sf_floatsidf(int32_t);
uint64_t sf_floatunsidf(uint32_t); int32_t sf_fixdfsi(uint64_t);
uint32_t sf_addsf3(uint32_t, uint32_t); uint32_t sf_subsf3(uint32_t, uint32_t); uint32_t sf_mulsf3(uint32_t, uint32_t);
uint32_t sf_divsf3(uint32_t, uint32_t);
int sf_eqsf2(uint32_t, uint32_t); int sf_nesf2(uint32_t, uint32_t); int sf_ltsf2(uint32_t, uint32_t);
int sf_lesf2(uint32_t, uint32_t); int sf_gtsf2(uint32_t, uint32_t); int sf_gesf2(uint32_t, uint32_t);
uint32_t sf_floatsisf(int32_t); int32_t sf_fixsfsi(uint32_t);

static double D(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
static uint64_t U(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static float Fl(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t UF(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* the soft-float result, canonical */
static uint64_t soft(int op, uint64_t a, uint64_t b, int *raw)
{
    uint32_t fa = (uint32_t)a, fb = (uint32_t)b;
    int r = 0;
    uint64_t v = 0;
    switch (op) {
    case OP_ADD: v = canon_d(sf_adddf3(a, b)); break;
    case OP_SUB: v = canon_d(sf_subdf3(a, b)); break;
    case OP_MUL: v = canon_d(sf_muldf3(a, b)); break;
    case OP_DIV: v = canon_d(sf_divdf3(a, b)); break;
    case OP_EQ: r = sf_eqdf2(a, b); v = rel(op, r); break;
    case OP_NE: r = sf_nedf2(a, b); v = rel(op, r); break;
    case OP_LT: r = sf_ltdf2(a, b); v = rel(op, r); break;
    case OP_LE: r = sf_ledf2(a, b); v = rel(op, r); break;
    case OP_GT: r = sf_gtdf2(a, b); v = rel(op, r); break;
    case OP_GE: r = sf_gedf2(a, b); v = rel(op, r); break;
    case OP_UNORD: r = sf_unorddf2(a, b); v = r != 0; break;
    case OP_EXT: v = canon_d(sf_extendsfdf2(fa)); break;
    case OP_TRUNC: case OP_TRUNCE: v = canon_f(sf_truncdfsf2(a)); break;
    case OP_I2D: v = sf_floatsidf((int32_t)fa); break;
    case OP_U2D: v = sf_floatunsidf(fa); break;
    case OP_D2I: v = (uint32_t)sf_fixdfsi(a); break;
    case OP_FADD: v = canon_f(sf_addsf3(fa, fb)); break;
    case OP_FSUB: v = canon_f(sf_subsf3(fa, fb)); break;
    case OP_FMUL: v = canon_f(sf_mulsf3(fa, fb)); break;
    case OP_FDIV: v = canon_f(sf_divsf3(fa, fb)); break;
    case OP_FEQ: r = sf_eqsf2(fa, fb); v = rel(op, r); break;
    case OP_FNE: r = sf_nesf2(fa, fb); v = rel(op, r); break;
    case OP_FLT: r = sf_ltsf2(fa, fb); v = rel(op, r); break;
    case OP_FLE: r = sf_lesf2(fa, fb); v = rel(op, r); break;
    case OP_FGT: r = sf_gtsf2(fa, fb); v = rel(op, r); break;
    case OP_FGE: r = sf_gesf2(fa, fb); v = rel(op, r); break;
    case OP_I2F: v = sf_floatsisf((int32_t)fa); break;
    case OP_F2I: v = (uint32_t)sf_fixsfsi(fa); break;
    }
    *raw = r;
    return v;
}

/* (int) as fp-bit gives it: NaN 0, beyond int32 the limit by sign */
static uint32_t d2i(double x)
{
    if (x != x) return 0;
    if (x >= 2147483648.0) return 0x7fffffffu;
    if (x <= -2147483649.0) return 0x80000000u;
    return (uint32_t)(int32_t)x;
}

static uint64_t fpu(int op, uint64_t a, uint64_t b)
{
    volatile double x = D(a), y = D(b);
    volatile float fx = Fl((uint32_t)a), fy = Fl((uint32_t)b);
    switch (op) {
    case OP_ADD: return canon_d(U(x + y));
    case OP_SUB: return canon_d(U(x - y));
    case OP_MUL: return canon_d(U(x * y));
    case OP_DIV: return canon_d(U(x / y));
    case OP_EQ: return x == y;
    case OP_NE: return !(x == y);
    case OP_LT: return x < y;
    case OP_LE: return x <= y;
    case OP_GT: return x > y;
    case OP_GE: return x >= y;
    case OP_UNORD: return x != x || y != y;
    case OP_EXT: return canon_d(U((double)fx));
    case OP_TRUNC: case OP_TRUNCE: return canon_f(UF((float)x));
    case OP_I2D: return U((double)(int32_t)(uint32_t)a);
    case OP_U2D: return U((double)(uint32_t)a);
    case OP_D2I: return d2i(x);
    case OP_FADD: return canon_f(UF(fx + fy));
    case OP_FSUB: return canon_f(UF(fx - fy));
    case OP_FMUL: return canon_f(UF(fx * fy));
    case OP_FDIV: return canon_f(UF(fx / fy));
    case OP_FEQ: return fx == fy;
    case OP_FNE: return !(fx == fy);
    case OP_FLT: return fx < fy;
    case OP_FLE: return fx <= fy;
    case OP_FGT: return fx > fy;
    case OP_FGE: return fx >= fy;
    case OP_I2F: return UF((float)(int32_t)(uint32_t)a);
    case OP_F2I: return d2i((double)fx);
    }
    return 0;
}

static int is_cmp(int op) { return (op >= OP_EQ && op <= OP_GE) || (op >= OP_FEQ && op <= OP_FGE); }
static int has_nan(int op, uint64_t a, uint64_t b)
{
    if (op >= OP_FEQ && op <= OP_FGE)
        return ((uint32_t)a & 0x7fffffffu) > 0x7f800000u || ((uint32_t)b & 0x7fffffffu) > 0x7f800000u;
    return (a & 0x7fffffffffffffffull) > 0x7ff0000000000000ull || (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull;
}

int main(int argc, char **argv)
{
    long n = argc > 1 ? atol(argv[1]) : 1000000, total = 0, bad = 0;
    uint64_t mask = argc > 2 ? strtoull(argv[2], 0, 0) : ~0ull;
    for (int op = 0; op < OP_N; op++) {
        uint32_t h = 2166136261u;
        long nb = 0, m = (mask >> op) & 1 ? cs_count(op, n) : 0;
        cs_state = 0x9e3779b97f4a7c15ull + (uint64_t)op;
        for (long k = 0; k < m; k++) {
            struct cs_case c;
            int raw = 0, ok;
            uint64_t s, f;
            cs_k = k;
            c = cs_case_for(op);
            if (cs_piece(op)) {                     /* the new y and yVel + yAcc: softfp.c's calls and the FPU */
                uint32_t fa = (uint32_t)c.a;
                uint64_t sv = sf_adddf3(c.b, c.c);
                volatile float fx = Fl(fa);
                volatile double v = D(c.b), a = D(c.c);
                s = canon_f(sf_truncdfsf2(sf_adddf3(sf_extendsfdf2(fa), c.b)));
                f = canon_f(UF((float)((double)fx + v)));
                ok = s == f && canon_d(sv) == canon_d(U(v + a));
                for (int i = 0; i < 64; i += 8) { h ^= (uint32_t)(canon_d(sv) >> i) & 0xff; h *= 16777619u; }
            } else {
                s = soft(op, c.a, c.b, &raw);
                f = fpu(op, c.a, c.b);
                ok = s == f;
            }
            if (ok && is_cmp(op)) {                 /* fp-bit's values: -1 / 0 / 1; NaN: 1, or -1 for gt / ge */
                if (raw < -1 || raw > 1) ok = 0;
                if (has_nan(op, c.a, c.b))
                    ok = raw == ((op == OP_GT || op == OP_GE || op == OP_FGT || op == OP_FGE) ? -1 : 1);
            }
            if (!ok && nb++ < 5)
                printf("  %s %016llx %016llx %016llx: soft %llx fpu %llx (raw %d)\n", cs_opname[op], (unsigned long long)c.a,
                       (unsigned long long)c.b, (unsigned long long)c.c, (unsigned long long)s, (unsigned long long)f, raw);
            for (int i = 0; i < 64; i += 8) { h ^= (uint32_t)(s >> i) & 0xff; h *= 16777619u; }
        }
        printf("%-12s %ld cases, %ld differ, hash %08x\n", cs_opname[op], m, nb, h);
        total += m;
        bad += nb;
    }
    printf("total %ld cases, %ld differ\n", total, bad);
    return bad != 0;
}
