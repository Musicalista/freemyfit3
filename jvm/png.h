#ifndef PNG_H
#define PNG_H
#include <stdint.h>
/* decodes a PNG; *ppx = malloc'd w*h RGB565, *palpha = malloc'd w*h alpha or NULL. returns 0 ok */
int png_decode(const uint8_t *d, uint32_t n, uint16_t **ppx, uint8_t **palpha, int *pw, int *ph);
#endif
