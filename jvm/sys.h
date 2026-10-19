/* sys.h: libc shim. On the watch (-DF3APP) there is no libc: memory comes from the F3Api and the few string functions are implemented in sys.c.
 * On the PC (tests) the real libc is used. */
#ifndef SYS_H
#define SYS_H
#include <stdint.h>
#include <stddef.h>
#ifdef F3APP
void *jm_alloc(uint32_t n);                 /* zeroed */
void jm_free(void *p);
void *jm_realloc(void *p, uint32_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
#define malloc(n) jm_alloc((uint32_t)(n))
#define calloc(a, b) jm_alloc((uint32_t)(a) * (uint32_t)(b))
#define realloc(p, n) jm_realloc((p), (uint32_t)(n))
#define free(p) jm_free(p)
double fmod(double a, double b);
float fmodf(float a, float b);
#else
#include <string.h>
#include <stdlib.h>
#include <math.h>
#endif
#endif
