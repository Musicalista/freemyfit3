/* Compact raw-DEFLATE decoder (RFC 1951), no libc beyond string.h. Returns bytes written or -1. */
#include "inflate.h"
#include <string.h>

typedef struct { const uint8_t *in; uint32_t inlen, incnt; uint32_t bitbuf; int bitcnt; uint8_t *out; uint32_t outlen, outcnt; int err; } St;
typedef struct { uint16_t count[16]; uint16_t symbol[288]; } Huff;

static int bits(St *s, int need) {
    uint32_t val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->incnt >= s->inlen) { s->err = 1; return 0; }
        val |= (uint32_t)s->in[s->incnt++] << s->bitcnt; s->bitcnt += 8;
    }
    s->bitbuf = val >> need; s->bitcnt -= need;
    return (int)(val & ((1u << need) - 1));
}
static int construct(Huff *h, const uint16_t *length, int n) {
    int sym, len, left; uint16_t offs[16];
    for (len = 0; len < 16; len++) h->count[len] = 0;
    for (sym = 0; sym < n; sym++) h->count[length[sym]]++;
    if (h->count[0] == n) return 0;
    left = 1;
    for (len = 1; len < 16; len++) { left <<= 1; left -= h->count[len]; if (left < 0) return left; }
    offs[1] = 0;
    for (len = 1; len < 15; len++) offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
    for (sym = 0; sym < n; sym++) if (length[sym]) h->symbol[offs[length[sym]]++] = (uint16_t)sym;
    return left;
}
static int decode(St *s, const Huff *h) {
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len < 16; len++) {
        code |= bits(s, 1); if (s->err) return -1;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -1;
}
static const uint16_t lbase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint16_t lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t dbase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint16_t dext[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int codes(St *s, const Huff *lc, const Huff *dc) {
    for (;;) {
        int sym = decode(s, lc); if (sym < 0) return -1;
        if (sym < 256) { if (s->outcnt >= s->outlen) return -1; s->out[s->outcnt++] = (uint8_t)sym; }
        else if (sym == 256) return 0;
        else {
            sym -= 257; if (sym >= 29) return -1;
            int len = lbase[sym] + bits(s, lext[sym]); if (s->err) return -1;
            int ds = decode(s, dc); if (ds < 0 || ds >= 30) return -1;
            uint32_t dist = dbase[ds] + (uint32_t)bits(s, dext[ds]); if (s->err) return -1;
            if (dist > s->outcnt || s->outcnt + len > s->outlen) return -1;
            while (len--) { s->out[s->outcnt] = s->out[s->outcnt - dist]; s->outcnt++; }
        }
    }
}
static int stored(St *s) {
    s->bitbuf = 0; s->bitcnt = 0;
    if (s->incnt + 4 > s->inlen) return -1;
    uint32_t len = s->in[s->incnt] | (s->in[s->incnt + 1] << 8);
    uint32_t nlen = s->in[s->incnt + 2] | (s->in[s->incnt + 3] << 8);
    if (len != (~nlen & 0xffff)) return -1;
    s->incnt += 4;
    if (s->incnt + len > s->inlen || s->outcnt + len > s->outlen) return -1;
    memcpy(s->out + s->outcnt, s->in + s->incnt, len); s->incnt += len; s->outcnt += len; return 0;
}
static int fixed(St *s) {
    static Huff lc, dc; static int built; uint16_t lengths[288]; int sym;
    if (!built) {
        for (sym = 0; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < 288; sym++) lengths[sym] = 8;
        construct(&lc, lengths, 288);
        for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
        construct(&dc, lengths, 30); built = 1;
    }
    return codes(s, &lc, &dc);
}
static int dynamic(St *s) {
    static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    uint16_t lengths[320]; Huff lc, dc; int index, err;
    int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
    if (s->err || nlen > 286 || ndist > 30) return -1;
    for (index = 0; index < ncode; index++) lengths[order[index]] = (uint16_t)bits(s, 3);
    for (; index < 19; index++) lengths[order[index]] = 0;
    if (construct(&lc, lengths, 19) != 0) return -1;
    index = 0;
    while (index < nlen + ndist) {
        int sym = decode(s, &lc), len, rep;
        if (sym < 0) return -1;
        if (sym < 16) lengths[index++] = (uint16_t)sym;
        else {
            len = 0;
            if (sym == 16) { if (index == 0) return -1; len = lengths[index - 1]; rep = 3 + bits(s, 2); }
            else if (sym == 17) rep = 3 + bits(s, 3); else rep = 11 + bits(s, 7);
            if (index + rep > nlen + ndist) return -1;
            while (rep--) lengths[index++] = (uint16_t)len;
        }
        if (s->err) return -1;
    }
    if (lengths[256] == 0) return -1;
    err = construct(&lc, lengths, nlen); if (err && (err < 0 || nlen != lc.count[0] + lc.count[1])) return -1;
    err = construct(&dc, lengths + nlen, ndist); if (err && (err < 0 || ndist != dc.count[0] + dc.count[1])) return -1;
    return codes(s, &lc, &dc);
}
int inflate_raw(const uint8_t *src, uint32_t slen, uint8_t *dst, uint32_t dlen) {
    St s; int last, type, r = 0;
    memset(&s, 0, sizeof s); s.in = src; s.inlen = slen; s.out = dst; s.outlen = dlen;
    do {
        last = bits(&s, 1); type = bits(&s, 2); if (s.err) return -1;
        r = type == 0 ? stored(&s) : type == 1 ? fixed(&s) : type == 2 ? dynamic(&s) : -1;
        if (r) return -1;
    } while (!last);
    return (int)s.outcnt;
}
