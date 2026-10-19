#include "chacha.h"

static uint32_t ld32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void st32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
#define ROTL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))
#define QR(a, b, c, d) a += b; d ^= a; d = ROTL(d, 16); c += d; b ^= c; b = ROTL(b, 12); a += b; d ^= a; d = ROTL(d, 8); c += d; b ^= c; b = ROTL(b, 7);

void cc20_block(const uint8_t key[32], uint32_t counter, const uint8_t nonce[12], uint8_t out[64]) {
    uint32_t s[16], x[16]; int i;
    s[0] = 0x61707865; s[1] = 0x3320646e; s[2] = 0x79622d32; s[3] = 0x6b206574;
    for (i = 0; i < 8; i++) s[4 + i] = ld32(key + 4 * i);
    s[12] = counter; s[13] = ld32(nonce); s[14] = ld32(nonce + 4); s[15] = ld32(nonce + 8);
    for (i = 0; i < 16; i++) x[i] = s[i];
    for (i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12]) QR(x[1], x[5], x[9], x[13]) QR(x[2], x[6], x[10], x[14]) QR(x[3], x[7], x[11], x[15])
        QR(x[0], x[5], x[10], x[15]) QR(x[1], x[6], x[11], x[12]) QR(x[2], x[7], x[8], x[13]) QR(x[3], x[4], x[9], x[14])
    }
    for (i = 0; i < 16; i++) st32(out + 4 * i, x[i] + s[i]);
}
static void cc20_xor(const uint8_t key[32], uint32_t counter, const uint8_t nonce[12], const uint8_t *in, uint8_t *out, int len) {
    uint8_t ks[64]; int i;
    while (len > 0) {
        int n = len < 64 ? len : 64;
        cc20_block(key, counter++, nonce, ks);
        for (i = 0; i < n; i++) out[i] = in[i] ^ ks[i];
        in += n; out += n; len -= n;
    }
}

/* ---- Poly1305 (26-bit limbs, as in poly1305-donna-32) ---- */
typedef struct { uint32_t r[5], h[5], pad[4]; uint8_t buf[16]; int fill; } Poly;
static void poly_init(Poly *p, const uint8_t k[32]) {
    p->r[0] = (ld32(k + 0)) & 0x3ffffff; p->r[1] = (ld32(k + 3) >> 2) & 0x3ffff03; p->r[2] = (ld32(k + 6) >> 4) & 0x3ffc0ff;
    p->r[3] = (ld32(k + 9) >> 6) & 0x3f03fff; p->r[4] = (ld32(k + 12) >> 8) & 0x00fffff;
    for (int i = 0; i < 5; i++) p->h[i] = 0;
    for (int i = 0; i < 4; i++) p->pad[i] = ld32(k + 16 + 4 * i);
    p->fill = 0;
}
static void poly_block(Poly *p, const uint8_t *m, uint32_t hibit) {
    uint32_t r0 = p->r[0], r1 = p->r[1], r2 = p->r[2], r3 = p->r[3], r4 = p->r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = p->h[0], h1 = p->h[1], h2 = p->h[2], h3 = p->h[3], h4 = p->h[4];
    uint64_t d0, d1, d2, d3, d4; uint32_t c;
    h0 += (ld32(m + 0)) & 0x3ffffff; h1 += (ld32(m + 3) >> 2) & 0x3ffffff; h2 += (ld32(m + 6) >> 4) & 0x3ffffff;
    h3 += (ld32(m + 9) >> 6) & 0x3ffffff; h4 += (ld32(m + 12) >> 8) | hibit;
    d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
    d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
    d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
    d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
    d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;
    c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff; d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
    d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff; d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
    d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff; h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;
    p->h[0] = h0; p->h[1] = h1; p->h[2] = h2; p->h[3] = h3; p->h[4] = h4;
}
static void poly_update(Poly *p, const uint8_t *m, int len) {
    while (len > 0) {
        int n = 16 - p->fill; if (n > len) n = len;
        for (int i = 0; i < n; i++) p->buf[p->fill + i] = m[i];
        p->fill += n; m += n; len -= n;
        if (p->fill == 16) { poly_block(p, p->buf, 1u << 24); p->fill = 0; }
    }
}
static void poly_pad16(Poly *p) { if (p->fill) { for (int i = p->fill; i < 16; i++) p->buf[i] = 0; poly_block(p, p->buf, 1u << 24); p->fill = 0; } }
static void poly_finish(Poly *p, uint8_t tag[16]) {
    uint32_t h0, h1, h2, h3, h4, c, g0, g1, g2, g3, g4, mask; uint64_t f;
    if (p->fill) {
        int i = p->fill; p->buf[i++] = 1; while (i < 16) p->buf[i++] = 0;
        poly_block(p, p->buf, 0);
    }
    h0 = p->h[0]; h1 = p->h[1]; h2 = p->h[2]; h3 = p->h[3]; h4 = p->h[4];
    c = h1 >> 26; h1 &= 0x3ffffff; h2 += c; c = h2 >> 26; h2 &= 0x3ffffff; h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff; h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;
    g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff; g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff; g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff; g4 = h4 + c - (1u << 26);
    mask = (g4 >> 31) - 1; g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask; mask = ~mask;
    h0 = (h0 & mask) | g0; h1 = (h1 & mask) | g1; h2 = (h2 & mask) | g2; h3 = (h3 & mask) | g3; h4 = (h4 & mask) | g4;
    h0 = (h0 | (h1 << 26)); h1 = ((h1 >> 6) | (h2 << 20)); h2 = ((h2 >> 12) | (h3 << 14)); h3 = ((h3 >> 18) | (h4 << 8));
    f = (uint64_t)h0 + p->pad[0]; h0 = (uint32_t)f; f = (uint64_t)h1 + p->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + p->pad[2] + (f >> 32); h2 = (uint32_t)f; f = (uint64_t)h3 + p->pad[3] + (f >> 32); h3 = (uint32_t)f;
    st32(tag, h0); st32(tag + 4, h1); st32(tag + 8, h2); st32(tag + 12, h3);
}

static void aead_tag(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, int aadlen, const uint8_t *ct, int len, uint8_t tag[16]) {
    uint8_t otk[64], lens[16]; Poly p; int i;
    cc20_block(key, 0, nonce, otk);
    poly_init(&p, otk);
    poly_update(&p, aad, aadlen); poly_pad16(&p);
    poly_update(&p, ct, len); poly_pad16(&p);
    for (i = 0; i < 8; i++) { lens[i] = (uint8_t)((uint32_t)aadlen >> (8 * (i & 3))); if (i >= 4) lens[i] = 0; }
    for (i = 0; i < 8; i++) { lens[8 + i] = (uint8_t)((uint32_t)len >> (8 * (i & 3))); if (i >= 4) lens[8 + i] = 0; }
    poly_update(&p, lens, 16);
    poly_finish(&p, tag);
}
void aead_seal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, int aadlen, const uint8_t *pt, int len, uint8_t *ct) {
    cc20_xor(key, 1, nonce, pt, ct, len);
    aead_tag(key, nonce, aad, aadlen, ct, len, ct + len);
}
int aead_open(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, int aadlen, const uint8_t *ct, int len, uint8_t *pt) {
    uint8_t tag[16]; int i; uint8_t diff = 0;
    aead_tag(key, nonce, aad, aadlen, ct, len, tag);
    for (i = 0; i < 16; i++) diff |= (uint8_t)(tag[i] ^ ct[len + i]);
    if (diff) return -1;
    cc20_xor(key, 1, nonce, ct, pt, len);
    return 0;
}
