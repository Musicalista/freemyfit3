/* libm-free single-precision helpers (target has no libc math). */
#ifndef RDMATH_H
#define RDMATH_H
#define RD_PI 3.14159265f
#define RD_DEG 0.017453292f

static inline float rd_sqrtf(float x) {
#if defined(__ARM_FP)
    return __builtin_sqrtf(x);          /* vsqrt.f32 */
#else
    if (x <= 0.0f) return 0.0f;
    union { float f; unsigned u; } c = { x }; c.u = 0x1fbd1df5u + (c.u >> 1);
    float y = c.f; y = 0.5f * (y + x / y); y = 0.5f * (y + x / y); y = 0.5f * (y + x / y); return y;
#endif
}
static inline float rd_floorf(float x) { int i = (int)x; return (float)(x < (float)i ? i - 1 : i); }
static inline float rd_sinf(float x) {
    x -= 2.0f * RD_PI * rd_floorf(x / (2.0f * RD_PI) + 0.5f);       /* [-pi, pi] */
    if (x > RD_PI * 0.5f) x = RD_PI - x; else if (x < -RD_PI * 0.5f) x = -RD_PI - x;
    float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f + x2 * (1.0f / 362880.0f)))));
}
static inline float rd_cosf(float x) { return rd_sinf(x + RD_PI * 0.5f); }
static inline float rd_tanf(float x) { return rd_sinf(x) / rd_cosf(x); }
#endif
