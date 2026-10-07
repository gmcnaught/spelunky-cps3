/* Real functions without libm (pmath.c): correctly rounded sqrt / sin / cos, glibc's atan2f. */
#ifndef PMATH_H
#define PMATH_H
#include <stdint.h>
double psqrt(double d);
double patan(double x);                       /* pscript.c (fdlibm s_atan.c) */
double point_distance_d(double x1, double y1, double x2, double y2);
double point_direction_d(double x1, double y1, double x2, double y2);
double degtorad_d(double d);
double psin_cr(double x);                     /* correctly rounded (glibc's sin / cos) */
double pcos_cr(double x);
void psincos_cr(double a, double *s, double *c);   /* both, the same bits, one reduction */
float patan2f(float y, float x);              /* glibc's atan2f (fdlibm float) */
/* point_distance(x1, y1, x2, y2) < c as GML compares it (DLT), from d2 = pdist2(...) without the square root
   (pworld.c: a threshold on d2 per c) */
static inline double pdist2(double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1, dy = y2 - y1;
    return dx * dx + dy * dy;                 /* point_distance_d's argument to psqrt */
}
int pdist2_lt(double d2, double c);
#endif
