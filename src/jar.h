#ifndef JAR_H
#define JAR_H
#include <stdint.h>
typedef struct { const uint8_t *data; uint32_t len, cd; int count; } Jar;
int jar_open(Jar *j, const uint8_t *data, uint32_t len);
uint8_t *jar_read(const Jar *j, const char *name, uint32_t *outlen);
#endif
