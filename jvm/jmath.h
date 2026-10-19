/* jmath.h: double-precision math for the watch (no libm there); on the PC the real libm is used. */
#ifndef JMATH_H
#define JMATH_H
#ifdef F3APP
double jm_sqrt(double x); double jm_sin(double x); double jm_cos(double x); double jm_tan(double x); double jm_atan(double x); double jm_atan2(double y, double x);
double jm_asin(double x); double jm_acos(double x); double jm_exp(double x); double jm_log(double x); double jm_pow(double a, double b); double jm_floor(double x); double jm_ceil(double x);
#else
#include <math.h>
#define jm_sqrt sqrt
#define jm_sin sin
#define jm_cos cos
#define jm_tan tan
#define jm_atan atan
#define jm_atan2 atan2
#define jm_asin asin
#define jm_acos acos
#define jm_exp exp
#define jm_log log
#define jm_pow pow
#define jm_floor floor
#define jm_ceil ceil
#endif
#endif
