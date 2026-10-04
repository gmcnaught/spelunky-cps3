/* Arithmetic of the play loop: GameMaker's reals (binary64) for the fractional game variables, selectable.
 *
 *   default            num = double, pos = float: bit-exact with GameMaker 2024.14, which computes every GML
 *                      real in binary64 and stores the built-in x, y, image_* in single precision (the
 *                      reference build of P4).
 *   -DPLAY_FIXED       num = s7.24 fixed point in an int32 (velocities, accelerations, gravity, friction: |v| < 128),
 *                      pos = s13.18 fixed point in an int32 (positions: |p| < 8192). Maldita's F16 formats.
 *
 * num: the GML variables xVel, yVel, xAcc, yAcc, grav, myGrav, gravityIntensity, friction factors, ...
 * pos: x, y. Most positions stay whole numbers (moveTo moves one pixel at a time), but rubble, poofs, flares and
 *      the like move by x += xVel.
 * GameMaker's image_index / image_speed are single-precision floats in the runner (Observed: image_index 2.4000000953674316
 * after 3 x image_speed 0.8 in build/trace/p4_walk_s1): type img_t (float in both builds).
 *
 * GML rules kept by the macros: round() rounds half to even (Observed in the runner: round(0.5) = 0, round(2.5) = 2,
 * round(-1.5) = -2; build/p4/probe); frac(x) keeps the sign (frac(-1.25) = -0.25); a mod b is C's fmod
 * ((-7) mod 3 = -1).
 */
#ifndef PNUM_H
#define PNUM_H
#include <stdint.h>

typedef float img_t;

static inline int32_t dfloor(double a) { int32_t i = (int32_t)a; return (a < (double)i) ? i - 1 : i; }
static inline int32_t dceil(double a) { int32_t i = (int32_t)a; return (a > (double)i) ? i + 1 : i; }
/* GML round(): half to even (the runner) */
static inline int32_t dround(double a)
{
    int32_t f = dfloor(a);
    double d = a - (double)f;
    if (d > 0.5) return f + 1;
    if (d < 0.5) return f;
    return (f & 1) ? f + 1 : f;
}

#if defined(PLAY_COUNT) && defined(__cplusplus)
/* -DPLAY_COUNT (compiled as C++ by test/host: playhost_count): num is a binary64 that counts its operations (the
   cost of keeping the GML reals in double on the SH-2, where each one is a libgcc soft-float call) */
struct dcount { unsigned long long add, mul, div, cmp, conv; };
extern struct dcount play_dcount;
struct num {
    double v;
    num() {}
    num(double d) : v(d) {}
    num(int i) : v(i) { play_dcount.conv++; }
    num(long i) : v((double)i) { play_dcount.conv++; }
    explicit operator double() const { return v; }
    num operator-() const { return num(-v); }
    num &operator+=(num o) { v += o.v; play_dcount.add++; return *this; }
    num &operator-=(num o) { v -= o.v; play_dcount.add++; return *this; }
};
static inline num operator+(num a, num b) { play_dcount.add++; return num(a.v + b.v); }
static inline num operator-(num a, num b) { play_dcount.add++; return num(a.v - b.v); }
static inline num operator*(num a, num b) { play_dcount.mul++; return num(a.v * b.v); }
static inline num operator/(num a, num b) { play_dcount.div++; return num(a.v / b.v); }
static inline bool operator<(num a, num b) { play_dcount.cmp++; return a.v < b.v; }
static inline bool operator>(num a, num b) { play_dcount.cmp++; return a.v > b.v; }
static inline bool operator<=(num a, num b) { play_dcount.cmp++; return a.v <= b.v; }
static inline bool operator>=(num a, num b) { play_dcount.cmp++; return a.v >= b.v; }
static inline bool operator==(num a, num b) { play_dcount.cmp++; return a.v == b.v; }
static inline bool operator!=(num a, num b) { play_dcount.cmp++; return a.v != b.v; }
#define NUM_IS_CLASS 1
#endif

#ifndef PLAY_FIXED
#ifndef NUM_IS_CLASS
typedef double num;
#endif
typedef float pos;                            /* the runner keeps x, y as floats (CInstance::SetPosition(float, float)) */
#define N(c)          ((num)(c))
#define NI(i)         ((num)(i))
#define NMUL(a, b)    ((a) * (b))
#define NDIV(a, b)    ((a) / (b))
#define NMULI(a, i)   ((a) * (num)(i))
#define NTOD(a)       ((double)(a))
#define ND(d)         ((num)(double)(d))      /* double -> num */
#define P(c)          ((pos)(c))
#define PI(i)         ((pos)(i))
#define PN(n)         ((pos)(n))              /* num -> pos */
/* x += v (GML): the double sum stored back as a float */
#define PADDN(dst, n) ((dst) = (pos)((double)(dst) + (double)(n)))
#define PSUBN(dst, n) ((dst) = (pos)((double)(dst) - (double)(n)))
#define NP(p)         ((num)(p))              /* pos -> num */
#define PTOD(p)       ((double)(p))
#define NFLOOR(a)     dfloor((double)(a))
#define NCEIL(a)      dceil((double)(a))
#define NROUND(a)     dround((double)(a))
#define PFLOOR(a)     dfloor(a)
#define PCEIL(a)      dceil(a)
#define PROUND(a)     dround(a)
#define NABS(a)       ((a) < 0 ? -(a) : (a))
/* GML frac(): x - trunc(x) */
static inline double dfrac(double a) { return a - (double)(int32_t)a; }
#define NFRAC(a)      ((num)dfrac((double)(a)))
/* 1 / a for a in (0, 1) (moveTo's round(1 / frac)) */
#define NRECIP_ROUND(a) dround((double)((num)1.0 / (a)))
#define NOPS(k)       (play_dops += (k))      /* binary64 operations counted (the SH-2 cost question) */
#else
typedef int32_t num;                          /* s7.24 */
typedef int32_t pos;                          /* s13.18 */
#define NFRAC_BITS 24
#define PFRAC_BITS 18
#define N(c)          ((num)((c) * 16777216.0 + ((c) < 0 ? -0.5 : 0.5)))
#define NI(i)         ((num)((int32_t)(i) * 16777216))
#define NMUL(a, b)    ((num)(((int64_t)(a) * (int64_t)(b)) >> 24))
#define NDIV(a, b)    ((num)((((int64_t)(a)) << 24) / (b)))
#define NMULI(a, i)   ((num)((a) * (int32_t)(i)))
#define NTOD(a)       ((double)(a) / 16777216.0)
#define ND(d)         ((num)((d) * 16777216.0 + ((d) < 0 ? -0.5 : 0.5)))
#define P(c)          ((pos)((c) * 262144.0 + ((c) < 0 ? -0.5 : 0.5)))
#define PI(i)         ((pos)((int32_t)(i) * 262144))
#define PN(n)         ((pos)((n) >> 6))
#define PADDN(dst, n) ((dst) += (pos)((n) >> 6))
#define PSUBN(dst, n) ((dst) -= (pos)((n) >> 6))
#define NP(p)         ((num)((p) << 6))
#define PTOD(p)       ((double)(p) / 262144.0)
static inline int32_t fx_floor(int32_t a, int b) { return a >> b; }
static inline int32_t fx_ceil(int32_t a, int b) { return -((-a) >> b); }
static inline int32_t fx_round(int32_t a, int b)
{
    int32_t f = a >> b, d = a - (f << b), h = 1 << (b - 1);
    if (d > h) return f + 1;
    if (d < h) return f;
    return (f & 1) ? f + 1 : f;
}
#define NFLOOR(a)     fx_floor((a), 24)
#define NCEIL(a)      fx_ceil((a), 24)
#define NROUND(a)     fx_round((a), 24)
#define PFLOOR(a)     fx_floor((a), 18)
#define PCEIL(a)      fx_ceil((a), 18)
#define PROUND(a)     fx_round((a), 18)
#define NABS(a)       ((a) < 0 ? -(a) : (a))
static inline num fx_frac(num a) { return a < 0 ? -((-a) & 0xFFFFFF) : (a & 0xFFFFFF); }
#define NFRAC(a)      fx_frac(a)
/* round(1 / a), a in (0, 1) s7.24: 2^48 / a in 24 fraction bits, rounded half to even */
static inline int32_t fx_recip_round(num a)
{
    int64_t q = (((int64_t)1) << 48) / a, r = (((int64_t)1) << 48) % a;
    int32_t f = (int32_t)(q >> 24);
    int64_t d = q & 0xFFFFFF;
    if (d > 0x800000 || (d == 0x800000 && r != 0)) return f + 1;
    if (d < 0x800000) return f;
    return (f & 1) ? f + 1 : f;
}
#define NRECIP_ROUND(a) fx_recip_round(a)
#define NOPS(k)       ((void)0)
#endif

/* GML comparisons of reals (the runner's YYCompareVal): d = a - b; equal when |d| <= epsilon (math_set_epsilon,
   default 0.00001), else the sign of d decides. Observed: build/trace/p4_push_rope_s365 record 168, a rope's
   yVel -4.2e-15 passes `yVel >= 0`. NLT .. NNE take two num (the fractional GML variables), DLT .. DNE two
   doubles (image_index, image_speed, positions) */
#define GML_EPS 0.00001
static inline int gcmp_d(double a, double b)
{
    double d = a - b;
    if ((d < 0 ? -d : d) <= GML_EPS) return 0;
    return d >= 0 ? 1 : -1;
}
#define DLT(a, b) (gcmp_d((double)(a), (double)(b)) < 0)
#define DLE(a, b) (gcmp_d((double)(a), (double)(b)) <= 0)
#define DGT(a, b) (gcmp_d((double)(a), (double)(b)) > 0)
#define DGE(a, b) (gcmp_d((double)(a), (double)(b)) >= 0)
#define DEQ(a, b) (gcmp_d((double)(a), (double)(b)) == 0)
#define DNE(a, b) (gcmp_d((double)(a), (double)(b)) != 0)
#ifndef PLAY_FIXED
#ifdef NUM_IS_CLASS
static inline int gcmp_n(num a, num b) { play_dcount.cmp++; return gcmp_d(a.v, b.v); }
#else
#define gcmp_n(a, b) gcmp_d((a), (b))
#endif
#else
static inline int gcmp_n(int32_t a, int32_t b)   /* epsilon 0.00001 = 168 / 2^24 */
{
    int32_t d = a - b;
    if ((d < 0 ? -d : d) <= 168) return 0;
    return d >= 0 ? 1 : -1;
}
#endif
#define NLT(a, b) (gcmp_n((a), (b)) < 0)
#define NLE(a, b) (gcmp_n((a), (b)) <= 0)
#define NGT(a, b) (gcmp_n((a), (b)) > 0)
#define NGE(a, b) (gcmp_n((a), (b)) >= 0)
#define NEQ(a, b) (gcmp_n((a), (b)) == 0)
#define NNE(a, b) (gcmp_n((a), (b)) != 0)

extern uint32_t play_dops;                    /* binary64 operations in the current step (double build) */

#endif
