/* Minimal ZIP/JAR reader over an in-memory image (stored + deflate only). */
#include "jar.h"
#include "inflate.h"
#include <stdlib.h>
#include <string.h>

static uint16_t r16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t r32(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

int jar_open(Jar *j, const uint8_t *data, uint32_t len) {
    int64_t i;
    memset(j, 0, sizeof *j); j->data = data; j->len = len;
    if (len < 22) return -1;
    for (i = (int64_t)len - 22; i >= 0 && i >= (int64_t)len - 65557; i--)
        if (r32(data + i) == 0x06054b50) break;
    if (i < 0 || r32(data + i) != 0x06054b50) return -1;
    j->count = r16(data + i + 10); j->cd = r32(data + i + 16);
    return j->cd < len ? 0 : -1;
}
/* returns malloc'd buffer (caller keeps), *outlen = size; NULL if missing/corrupt */
uint8_t *jar_read(const Jar *j, const char *name, uint32_t *outlen) {
    const uint8_t *p = j->data + j->cd; size_t nl = strlen(name);
    for (int k = 0; k < j->count; k++) {
        if (r32(p) != 0x02014b50) return 0;
        uint16_t method = r16(p + 10), fn = r16(p + 28), ex = r16(p + 30), cm = r16(p + 32);
        uint32_t csz = r32(p + 20), usz = r32(p + 24), lho = r32(p + 42);
        if (fn == nl && !memcmp(p + 46, name, nl)) {
            const uint8_t *l = j->data + lho; if (r32(l) != 0x04034b50) return 0;
            const uint8_t *d = l + 30 + r16(l + 26) + r16(l + 28);
            uint8_t *out = malloc(usz ? usz : 1); if (!out) return 0;
            if (method == 0) memcpy(out, d, usz);
            else if (method != 8 || inflate_raw(d, csz, out, usz) != (int)usz) { free(out); return 0; }
            *outlen = usz; return out;
        }
        p += 46 + fn + ex + cm;
    }
    return 0;
}
