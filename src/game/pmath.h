/* Real functions without libm (pmath.c): fdlibm sin / cos / atan2, a correctly rounded sqrt. */
#ifndef PMATH_H
#define PMATH_H
#include <stdint.h>
double psqrt(double d);
double psin(double x);
double pcos(double x);
double patan(double x);                       /* pscript.c (fdlibm s_atan.c) */
double patan2(double y, double x);
double point_distance_d(double x1, double y1, double x2, double y2);
double point_direction_d(double x1, double y1, double x2, double y2);
double degtorad_d(double d);
double psin_cr(double x);                     /* correctly rounded (glibc's sin / cos) */
double pcos_cr(double x);
float patan2f(float y, float x);              /* glibc's atan2f (fdlibm float) */
#endif
