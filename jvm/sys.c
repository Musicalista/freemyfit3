/* sys.c: libc replacements for the watch build (-DF3APP). The allocator comes from the F3Api through jm_set_alloc(). */
#ifdef F3APP
#include "sys.h"
#include "jmath.h"
static void *(*g_malloc)(uint32_t); static void (*g_free)(void *);
void jm_set_alloc(void *(*m)(uint32_t), void (*f)(void *)) { g_malloc = m; g_free = f; }
void *memcpy(void *d, const void *s, size_t n) {
    uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s;
    if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) { uint32_t *wa = (uint32_t *)a; const uint32_t *wb = (const uint32_t *)b; while (n >= 4) { *wa++ = *wb++; n -= 4; } a = (uint8_t *)wa; b = (const uint8_t *)wb; }
    while (n--) *a++ = *b++;
    return d;
}
void *memmove(void *d, const void *s, size_t n) {
    uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s;
    if (a < b || a >= b + n) return memcpy(d, s, n);
    a += n; b += n; while (n--) *--a = *--b;
    return d;
}
void *memset(void *d, int c, size_t n) {
    uint8_t *a = (uint8_t *)d; uint32_t w = (uint8_t)c * 0x01010101u;
    while (n && ((uintptr_t)a & 3)) { *a++ = (uint8_t)c; n--; }
    { uint32_t *wa = (uint32_t *)a; while (n >= 4) { *wa++ = w; n -= 4; } a = (uint8_t *)wa; }
    while (n--) *a++ = (uint8_t)c;
    return d;
}
int memcmp(const void *x, const void *y, size_t n) { const uint8_t *a = (const uint8_t *)x, *b = (const uint8_t *)y; for (; n; n--, a++, b++) if (*a != *b) return *a - *b; return 0; }
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (uint8_t)*a - (uint8_t)*b; }
int strncmp(const char *a, const char *b, size_t n) { for (; n; n--, a++, b++) { if (*a != *b) return (uint8_t)*a - (uint8_t)*b; if (!*a) return 0; } return 0; }
char *strchr(const char *s, int c) { for (; *s; s++) if (*s == (char)c) return (char *)s; return c ? 0 : (char *)s; }
void *jm_alloc(uint32_t n) { uint32_t *p = (uint32_t *)g_malloc(n + 8); if (!p) return 0; p[0] = n; memset(p + 2, 0, n); return p + 2; }
void jm_free(void *q) { if (q) g_free((uint32_t *)q - 2); }
void *jm_realloc(void *q, uint32_t n) { if (!q) return jm_alloc(n); uint32_t old = ((uint32_t *)q)[-2]; void *r = jm_alloc(n); if (!r) return 0; memcpy(r, q, old < n ? old : n); jm_free(q); return r; }

/* ---------- math ---------- */
#define PI 3.14159265358979323846
#define LN2 0.69314718055994530942
static double fabs_(double x) { return x < 0 ? -x : x; }
double jm_floor(double x) { if (x != x || fabs_(x) >= 4503599627370496.0) return x; int64_t i = (int64_t)x; double d = (double)i; return d > x ? d - 1.0 : d; }
double jm_ceil(double x) { double f = jm_floor(x); return f < x ? f + 1.0 : f; }
double jm_sqrt(double x) {
    if (!(x > 0)) return x == 0 ? 0 : (x - x) / (x - x);
    union { double d; uint64_t u; } v; v.d = x; v.u = (v.u >> 1) + 0x1ff8000000000000ull; double g = v.d;
    for (int i = 0; i < 6; i++) g = 0.5 * (g + x / g);
    return g;
}
double fmod(double a, double b) { if (b == 0 || a != a || b != b) return (a - a) / (a - a); double q = a / b; double t = q < 0 ? jm_ceil(q) : jm_floor(q); return a - t * b; }
float fmodf(float a, float b) { return (float)fmod(a, b); }
static double sin_core(double x) {                    /* |x| <= pi/2 */
    double x2 = x * x;
    return x * (1 + x2 * (-1.0 / 6 + x2 * (1.0 / 120 + x2 * (-1.0 / 5040 + x2 * (1.0 / 362880 + x2 * (-1.0 / 39916800 + x2 * (1.0 / 6227020800.0)))))));
}
double jm_sin(double x) {
    if (x != x) return x;
    x = fmod(x, 2 * PI); if (x > PI) x -= 2 * PI; if (x < -PI) x += 2 * PI;
    if (x > PI / 2) x = PI - x; else if (x < -PI / 2) x = -PI - x;
    return sin_core(x);
}
double jm_cos(double x) { return jm_sin(x + PI / 2); }
double jm_atan(double x) {
    if (x != x) return x;
    int neg = x < 0; if (neg) x = -x;
    int inv = x > 1; if (inv) x = 1 / x;
    double add = 0;
    if (x > 0.2679491924311227) { x = (x - 0.5773502691896258) / (1 + x * 0.5773502691896258); add = PI / 6; }
    double x2 = x * x, r = x * (1 + x2 * (-1.0 / 3 + x2 * (1.0 / 5 + x2 * (-1.0 / 7 + x2 * (1.0 / 9 + x2 * (-1.0 / 11 + x2 * (1.0 / 13)))))));
    r += add; if (inv) r = PI / 2 - r;
    return neg ? -r : r;
}
double jm_atan2(double y, double x) {
    if (x > 0) return jm_atan(y / x);
    if (x < 0) return y >= 0 ? jm_atan(y / x) + PI : jm_atan(y / x) - PI;
    return y > 0 ? PI / 2 : y < 0 ? -PI / 2 : 0;
}
#endif
