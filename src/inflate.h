#ifndef INFLATE_H
#define INFLATE_H
#include <stdint.h>
/* decode raw DEFLATE stream; returns number of bytes written to dst, or -1 on error */
int inflate_raw(const uint8_t *src, uint32_t slen, uint8_t *dst, uint32_t dlen);
#endif
