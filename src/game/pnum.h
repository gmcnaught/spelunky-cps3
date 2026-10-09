/* Arithmetic of the play loop: GameMaker's reals (binary64) for the fractional game variables.
 *
 *   num = double, pos = float: bit-exact with GameMaker 2024.14, which computes every GML real in binary64 and stores
 *   the built-in x, y, image_* in single precision. (The s7.24 / s13.18 fixed-point build, PLAY_FIXED, changed
 *   positions on 5 of 6 P4 routes, PLAN.md P4, and was removed.) PLAY_COUNT (C++): num a class counting operations.
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

/* (double)f for a float on the bits (no __extendsfdf2 call): a normal number's exponent rebiased (127 -> 1023) and its
   mantissa shifted into place; +-0 the sign alone; subnormals, infinities and NaN through the conversion. TOD(x):
   (double)x, with fwiden when x is a float (the type picks it at compile time; C++: the plain conversion) */
static inline double fwiden(float f)
{
    union { float f; uint32_t u; } v;
    union { double d; uint64_t u; } r;
    uint32_t e;
    v.f = f;
    e = (v.u >> 23) & 0xffu;
    if (e == 0 || e == 0xffu) {
        if ((v.u << 1) != 0) return (double)f;
        r.u = (uint64_t)(v.u & 0x80000000u) << 32;
        return r.d;
    }
    r.u = ((uint64_t)((v.u & 0x80000000u) | ((e + 896u) << 20) | ((v.u & 0x7fffffu) >> 3)) << 32) | (uint64_t)(v.u << 29);
    return r.d;
}
#ifdef __cplusplus
#define TOD(x) ((double)(x))
#else
#define TOD(x) __builtin_choose_expr(__builtin_types_compatible_p(__typeof__(x), float), fwiden((float)(x)), (double)(x))
#endif

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
#define PADDV(cur, n) ((pos)((double)(cur) + (double)(n)))     /* the value PADDN stores (pin_setx / pin_sety) */
#define PSUBV(cur, n) ((pos)((double)(cur) - (double)(n)))
#define NP(p)         ((num)(p))              /* pos -> num */
#define PTOD(p)       ((double)(p))
#define NFLOOR(a)     dfloor((double)(a))
#define NCEIL(a)      dceil((double)(a))
#define NROUND(a)     dround((double)(a))
#define PFLOOR(a)     dfloor(a)
#define PCEIL(a)      dceil(a)
#define PROUND(a)     dround(a)
#ifdef NUM_IS_CLASS
#define NABS(a)       ((a) < 0 ? -(a) : (a))
#else
/* (a < 0 ? -a : a) on the bits: the sign cleared for a negative, non-zero, non-NaN a (-0 and NaN unchanged, as the
   compare leaves them) */
static inline double dnabs(double a)
{
    union { double d; uint64_t u; } v;
    uint64_t m;
    v.d = a;
    m = v.u & 0x7fffffffffffffffull;
    if ((v.u >> 63) && m != 0 && m <= 0x7ff0000000000000ull) v.u = m;
    return v.d;
}
#define NABS(a)       dnabs((double)(a))
#endif
/* GML frac(): x - trunc(x) */
static inline double dfrac(double a) { return a - (double)(int32_t)a; }
#define NFRAC(a)      ((num)dfrac((double)(a)))
/* 1 / a for a in (0, 1) (moveTo's round(1 / frac)) */
#define NRECIP_ROUND(a) dround((double)((num)1.0 / (a)))
#define NOPS(k)       (play_dops += (k))      /* binary64 operations counted (the SH-2 cost question) */

/* bit tests without soft-float (the SH-2 has no FPU): d == 0 (either sign), and a float that is a whole number below
   2^15 in magnitude as its int (the mantissa times 2^(e - 127 + 9) has the integer part in the high word and the
   fraction in the low one: a 32 x 32 -> 64 multiply, no variable shift) */
static inline int dzero(double d)
{
    union { double d; uint64_t u; } v;
    v.d = d;
    return (v.u << 1) == 0;
}
/* dzero((double)f) on the float's bits (the widening keeps +-0 and every other value: no __extendsfdf2 call) */
static inline int fzero(float f)
{
    union { float f; uint32_t u; } v;
    v.f = f;
    return (v.u << 1) == 0;
}
static const uint32_t fwhole_mul[15] = { 1u << 9, 1u << 10, 1u << 11, 1u << 12, 1u << 13, 1u << 14, 1u << 15,
                                         1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22,
                                         1u << 23 };
static inline int fwhole(float f, int32_t *o)
{
    union { float f; uint32_t u; } v;
    uint32_t e;
    uint64_t p;
    v.f = f;
    if ((v.u & 0x7fffffffu) == 0) { *o = 0; return 1; }
    e = (v.u >> 23) & 0xffu;
    if (e < 127 || e > 141) return 0;
    p = (uint64_t)((v.u & 0x7fffffu) | 0x800000u) * fwhole_mul[e - 127];
    if ((uint32_t)p != 0) return 0;
    *o = (v.u & 0x80000000u) ? -(int32_t)(p >> 32) : (int32_t)(p >> 32);
    return 1;
}

/* (float)v for |v| < 2^15 from the bits (no __floatsisf call): the top bit's place e by four compares, the mantissa as
   a * 2^(23 - e) (exact: a < 2^(e + 1)), its hidden bit added to the exponent field 126 + e. Checked equal to
   (float)v for every |v| < 2^15 (pworld.c pw_xstep, PLACE_F; pscript.c moveTo's walks) */
static const uint32_t fi_mul[16] = { 1u << 23, 1u << 22, 1u << 21, 1u << 20, 1u << 19, 1u << 18, 1u << 17, 1u << 16,
                                     1u << 15, 1u << 14, 1u << 13, 1u << 12, 1u << 11, 1u << 10, 1u << 9, 1u << 8 };
static inline float fint15(int32_t v)
{
    union { float f; uint32_t u; } r;
    uint32_t a = v < 0 ? (uint32_t)-v : (uint32_t)v, t = a, e = 0;
    if (a == 0) return 0.0f;
    if (t >= 0x100) { e = 8; t >>= 8; }
    if (t >= 0x10) { e += 4; t >>= 4; }
    if (t >= 0x4) { e += 2; t >>= 2; }
    if (t >= 0x2) e += 1;
    r.u = ((126 + e) << 23) + a * fi_mul[e];
    if (v < 0) r.u |= 0x80000000u;
    return r.f;
}
/* sprite_width / sprite_height: (int)(w * s) for a sprite size w (0 <= w < 2^15) and a float scale s. s = +-k whole,
   k <= 256 (fwhole): |w k| < 2^23, so the float product is w k exactly; other s take the float product */
static inline int spr_dim(int w, float s)
{
    int32_t k;
    if (fwhole(s, &k) && k >= -256 && k <= 256) return w * k;
    return (int)(w * s);
}

/* GML comparisons of reals (the runner's YYCompareVal): d = a - b; equal when |d| <= epsilon (math_set_epsilon,
   default 0.00001), else the sign of d decides. Observed: build/trace/p4_push_rope_s365 record 168, a rope's
   yVel -4.2e-15 passes `yVel >= 0`. NLT .. NNE take two num (the fractional GML variables), DLT .. DNE two
   doubles (image_index, image_speed, positions) */
#define GML_EPS 0.00001
#define GML_EPS_BITS 0x3ee4f8b588e368f1ull    /* the bits of GML_EPS */
/* gcmp_dd(a, 0) on the bits: a - 0 is a; |a| <= eps compares as unsigned (the magnitude bits order as the values,
   NaN above infinity); then NaN gives -1 (d >= 0 is false) and the sign bit decides */
static inline int gcmp_z(double a)
{
    union { double d; uint64_t u; } v;
    uint64_t m;
    v.d = a;
    m = v.u & 0x7fffffffffffffffull;
    if (m <= GML_EPS_BITS) return 0;
    if (m > 0x7ff0000000000000ull) return -1;
    return (v.u >> 63) ? -1 : 1;
}
/* d = a - b; 0 when |d| <= eps, else the sign of d (NaN: -1). The tests on d are gcmp_z's on its bits (one soft-float
   call, the subtraction, in place of three) */
static inline int gcmp_dd(double a, double b)
{
    return gcmp_z(a - b);
}
#define gcmp_d(a, b)  ((__builtin_constant_p(b) && (b) == 0) ? gcmp_z(a) : gcmp_dd((a), (b)))
/* gcmp_dd(a * m, 0) > 0 for an int m. For m = 2^k (k <= 20) the product is exact unless it overflows (then
   +-inf, on the same side of eps), so for a >= 0 it is a > eps / 2^k: the bits of eps with k taken off the exponent */
static inline int gpos_muli_gt0(double a, int32_t m)
{
    union { double d; uint64_t u; } v;
    uint32_t k = 0, t = (uint32_t)m;
    if (m <= 0 || m > (1 << 20) || (t & (t - 1)) != 0) return gcmp_dd(a * (double)m, 0) > 0;
    while (t > 1) { t >>= 1; k++; }
    v.d = a;
    if (v.u >> 63) return 0;                              /* a <= 0 (-0 included), or a NaN with the sign: <= 0 / NaN */
    if (v.u > 0x7ff0000000000000ull) return 0;            /* NaN */
    return v.u > GML_EPS_BITS - ((uint64_t)k << 52);
}
#define DLT(a, b) (gcmp_d((double)(a), (double)(b)) < 0)
#define DLE(a, b) (gcmp_d((double)(a), (double)(b)) <= 0)
#define DGT(a, b) (gcmp_d((double)(a), (double)(b)) > 0)
#define DGE(a, b) (gcmp_d((double)(a), (double)(b)) >= 0)
#define DEQ(a, b) (gcmp_d((double)(a), (double)(b)) == 0)
#define DNE(a, b) (gcmp_d((double)(a), (double)(b)) != 0)
/* gcmp_d(PTOD(x), v) for a position x and an int v (the view tests). Float x: d = x - v is exact as a double when |d|
   is near eps (x a float, v an int), and the rounding of a larger d keeps it beyond eps, so the exact d decides. For
   2^-9 <= |x| < 2^23 the float is +-(h + l / 2^32) on its bits (fwhole's multiply; l a 2^-32 fraction) and eps is
   42949.67 / 2^32: d > eps is h - v >= 1 or (h == v and l >= 42950); d < -eps is h - v <= -2 or (h - v == -1 and
   1 - l / 2^32 >= 42950 / 2^32). A negative x is -gcmp(|x|, -v). Other x (and |v| >= 2^30) take the double form */
int gcmp_fi(float x, int32_t v);                    /* pworld.c (out of line: four tests a view check) */
/* gcmp_dd out of line, for rare compares inside large hot functions: inlined there, gcmp_dd changed GCC's register
   allocation of the whole function (characterStepEvent's ladder snap: jtcps3 route steps +1 to +3 %) */
int gcmp_cold(double a, double b);                  /* pworld.c */
#define NLT_COLD(a, b) (gcmp_cold(NTOD(a), NTOD(b)) < 0)
#define NGT_COLD(a, b) (gcmp_cold(NTOD(a), NTOD(b)) > 0)
#define PLTI(x, v) (gcmp_fi((x), (v)) < 0)
#define PGTI(x, v) (gcmp_fi((x), (v)) > 0)
int gout_fi(float x, int32_t lo, int32_t hi);       /* pworld.c: PLTI(x, lo) || PGTI(x, hi), x decoded once */
#define POUTI(x, lo, hi) gout_fi((x), (lo), (hi))
/* dround((double)x + k) for a position x (calculateCollisionBounds' sides plus the scripts' offsets, rounded) in two
   steps without soft-float: pfr(x, &v) packs floor(x) * 4 + the class of x - floor(x) (0: zero, 1: below 1/2, 2: 1/2,
   3: above); 0 (nothing set) when |x| >= 2^22 or x is not finite. pfr_k(v, k) is then dround((double)x + k) for
   |k| < 2^20: x + k is exact as a double (at most 47 bits apart: x's lowest bit is >= 2^-24 when |x| >= 1/2), its
   floor is floor(x) + k and its fraction x's; for |x| < 1/2 pfr gives 0 (floor 0, fraction 0): the rounded sum is k
   (the double sum lies strictly between k - 1/2 and k + 1/2) */
int pfr(float x, int32_t *o);                        /* pworld.c */
static inline int32_t pfr_k(int32_t v, int32_t k)
{
    int32_t f = (v >> 2) + k;
    int c = v & 3;
    return f + (c == 3 || (c == 2 && (f & 1)));
}
#ifdef NUM_IS_CLASS
static inline int gcmp_n(num a, num b) { play_dcount.cmp++; return gcmp_dd(a.v, b.v); }
#else
#define gcmp_n(a, b) gcmp_d((a), (b))
#endif
#define NLT(a, b) (gcmp_n((a), (b)) < 0)
#define NLE(a, b) (gcmp_n((a), (b)) <= 0)
#define NGT(a, b) (gcmp_n((a), (b)) > 0)
#define NGE(a, b) (gcmp_n((a), (b)) >= 0)
#define NEQ(a, b) (gcmp_n((a), (b)) == 0)
#define NNE(a, b) (gcmp_n((a), (b)) != 0)
#if !defined(NUM_IS_CLASS)
#define NMULI_GT0(a, i) gpos_muli_gt0((double)(a), (i))        /* NGT(NMULI(a, i), N(0)) */
#else
#define NMULI_GT0(a, i) NGT(NMULI((a), (i)), N(0))
#endif

extern uint32_t play_dops;                    /* binary64 operations in the current step (double build) */

#endif
