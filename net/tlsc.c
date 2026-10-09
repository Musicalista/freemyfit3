/* TLS 1.3 client, see tlsc.h. SHA-256, HMAC, HKDF, X25519 (TweetNaCl-style field arithmetic: small, slow but fine for one handshake) and the handshake state machine. */
#include "tlsc.h"
#include "chacha.h"

static void tw_cpy(uint8_t *d, const uint8_t *s, int n) { for (int i = 0; i < n; i++) d[i] = s[i]; }
static void tw_zer(uint8_t *d, int n) { for (int i = 0; i < n; i++) d[i] = 0; }
static int tw_slen(const char *s) { int n = 0; while (s[n]) n++; return n; }

/* ---------------- SHA-256 ---------------- */
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha256 *s, const uint8_t *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) | ((uint32_t)p[4 * i + 2] << 8) | p[4 * i + 3];
    for (int i = 16; i < 64; i++) { uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10); w[i] = w[i - 16] + s0 + w[i - 7] + s1; }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i], t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
void sha256_init(Sha256 *s) {
    static const uint32_t iv[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    for (int i = 0; i < 8; i++) s->h[i] = iv[i]; s->total = 0; s->n = 0;
}
void sha256_update(Sha256 *s, const uint8_t *d, int n) {
    s->total += (uint64_t)n;
    while (n > 0) { int k = 64 - s->n; if (k > n) k = n; tw_cpy(s->buf + s->n, d, k); s->n += k; d += k; n -= k; if (s->n == 64) { sha_block(s, s->buf); s->n = 0; } }
}
static void sha_copy(Sha256 *d, const Sha256 *s) { for (int i = 0; i < 8; i++) d->h[i] = s->h[i]; d->total = s->total; d->n = s->n; tw_cpy(d->buf, s->buf, 64); }
void sha256_final(Sha256 *s, uint8_t out[32]) {
    uint64_t bits = s->total * 8; uint8_t pad = 0x80; sha256_update(s, &pad, 1); pad = 0; while (s->n != 56) sha256_update(s, &pad, 1);
    uint8_t l[8]; for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i)); sha256_update(s, l, 8);
    for (int i = 0; i < 8; i++) { out[4 * i] = (uint8_t)(s->h[i] >> 24); out[4 * i + 1] = (uint8_t)(s->h[i] >> 16); out[4 * i + 2] = (uint8_t)(s->h[i] >> 8); out[4 * i + 3] = (uint8_t)s->h[i]; }
}
static void sha_snapshot(const Sha256 *s, uint8_t out[32]) { Sha256 c; sha_copy(&c, s); sha256_final(&c, out); }

/* ---------------- HMAC / HKDF ---------------- */
void hmac_sha256(const uint8_t *key, int klen, const uint8_t *a, int alen, const uint8_t *b, int blen, uint8_t out[32]) {
    uint8_t k[64], ip[64], op[64], inner[32]; Sha256 s;
    tw_zer(k, 64); if (klen > 64) { sha256_init(&s); sha256_update(&s, key, klen); sha256_final(&s, k); } else tw_cpy(k, key, klen);
    for (int i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    sha256_init(&s); sha256_update(&s, ip, 64); if (alen) sha256_update(&s, a, alen); if (blen) sha256_update(&s, b, blen); sha256_final(&s, inner);
    sha256_init(&s); sha256_update(&s, op, 64); sha256_update(&s, inner, 32); sha256_final(&s, out);
}
static void expand_label(const uint8_t secret[32], const char *label, const uint8_t *ctx, int ctxlen, uint8_t *out, int olen) {   /* HKDF-Expand-Label, olen <= 32 */
    uint8_t info[80]; int n = 0, ll = tw_slen(label);
    info[n++] = (uint8_t)(olen >> 8); info[n++] = (uint8_t)olen; info[n++] = (uint8_t)(6 + ll);
    const char *pre = "tls13 "; for (int i = 0; i < 6; i++) info[n++] = (uint8_t)pre[i]; for (int i = 0; i < ll; i++) info[n++] = (uint8_t)label[i];
    info[n++] = (uint8_t)ctxlen; for (int i = 0; i < ctxlen; i++) info[n++] = ctx[i];
    uint8_t one = 1, t[32]; hmac_sha256(secret, 32, info, n, &one, 1, t); tw_cpy(out, t, olen);
}
static void traffic_keys(const uint8_t secret[32], uint8_t key[32], uint8_t iv[12]) { expand_label(secret, "key", 0, 0, key, 32); expand_label(secret, "iv", 0, 0, iv, 12); }

/* ---------------- X25519 ---------------- */
typedef int64_t gf[16];
static void car25519(gf o) { int64_t c; for (int i = 0; i < 16; i++) { o[i] += (1LL << 16); c = o[i] >> 16; o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15); o[i] -= c << 16; } }
static void sel25519(gf p, gf q, int b) { int64_t t, c = ~((int64_t)b - 1); for (int i = 0; i < 16; i++) { t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; } }
static void pack25519(uint8_t *o, const gf n) {
    int b; gf m, t; for (int i = 0; i < 16; i++) t[i] = n[i]; car25519(t); car25519(t); car25519(t);
    for (int j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed; for (int i = 1; i < 15; i++) { m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1); m[i - 1] &= 0xffff; }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1); b = (int)((m[15] >> 16) & 1); m[14] &= 0xffff; sel25519(t, m, 1 - b);
    }
    for (int i = 0; i < 16; i++) { o[2 * i] = (uint8_t)(t[i] & 0xff); o[2 * i + 1] = (uint8_t)(t[i] >> 8); }
}
static void unpack25519(gf o, const uint8_t *n) { for (int i = 0; i < 16; i++) o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8); o[15] &= 0x7fff; }
static void fadd(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void fsub(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
static void fmul(gf o, const gf a, const gf b) {
    int64_t t[31]; for (int i = 0; i < 31; i++) t[i] = 0;
    for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; i++) o[i] = t[i]; car25519(o); car25519(o);
}
static void fsq(gf o, const gf a) { fmul(o, a, a); }
static void finv(gf o, const gf in) { gf c; for (int a = 0; a < 16; a++) c[a] = in[a]; for (int a = 253; a >= 0; a--) { fsq(c, c); if (a != 2 && a != 4) fmul(c, c, in); } for (int a = 0; a < 16; a++) o[a] = c[a]; }
void x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) {
    static const gf k121665 = { 0xDB41, 1 };
    uint8_t z[32]; int64_t x[80], r; gf a, b, c, d, e, f;
    for (int i = 0; i < 31; i++) z[i] = scalar[i]; z[31] = (uint8_t)((scalar[31] & 127) | 64); z[0] &= 248;
    unpack25519(x, point);
    for (int i = 0; i < 16; i++) { b[i] = x[i]; d[i] = a[i] = c[i] = 0; } a[0] = d[0] = 1;
    for (int i = 254; i >= 0; --i) {
        r = (z[i >> 3] >> (i & 7)) & 1; sel25519(a, b, (int)r); sel25519(c, d, (int)r);
        fadd(e, a, c); fsub(a, a, c); fadd(c, b, d); fsub(b, b, d); fsq(d, e); fsq(f, a); fmul(a, c, a); fmul(c, b, e); fadd(e, a, c); fsub(a, a, c); fsq(b, a); fsub(c, d, f);
        fmul(a, c, k121665); fadd(a, a, d); fmul(c, c, a); fmul(a, d, f); fmul(d, b, x); fsq(b, e); sel25519(a, b, (int)r); sel25519(c, d, (int)r);
    }
    for (int i = 0; i < 16; i++) { x[i + 16] = a[i]; x[i + 32] = c[i]; x[i + 48] = b[i]; x[i + 64] = d[i]; }
    finv(x + 32, x + 32); fmul(x + 16, x + 16, x + 32); pack25519(out, x + 16);
}

/* ---------------- TLS 1.3 ---------------- */
static void p16(uint8_t *b, int *n, int v) { b[(*n)++] = (uint8_t)(v >> 8); b[(*n)++] = (uint8_t)v; }
static void pbytes(uint8_t *b, int *n, const uint8_t *s, int l) { tw_cpy(b + *n, s, l); *n += l; }
Tls *tls_new(void *(*alloc)(uint32_t), void (*release)(void *)) {
    Tls *t = (Tls *)alloc(sizeof(Tls)); if (!t) return 0;
    tw_zer((uint8_t *)t, (int)sizeof *t);
    t->rec = (uint8_t *)alloc(TLS_REC_MAX); if (!t->rec) { release(t); return 0; }
    return t;
}
void tls_free(Tls *t, void (*release)(void *)) { if (!t) return; if (t->rec) release(t->rec); release(t); }
static void set_err(Tls *t, int e) { t->state = TLS_ERR; t->err = e; }
static void nonce_for(const uint8_t iv[12], uint64_t seq, uint8_t n[12]) { for (int i = 0; i < 12; i++) n[i] = iv[i]; for (int i = 0; i < 8; i++) n[11 - i] ^= (uint8_t)(seq >> (8 * i)); }

int tls_start(Tls *t, const char *host, void (*rnd)(void *, uint8_t *, int), void *ctx) {
    uint8_t base[32], random[32], sid[32]; tw_zer(base, 32); base[0] = 9;
    t->rnd = rnd; t->rctx = ctx; rnd(ctx, t->priv, 32); rnd(ctx, random, 32); rnd(ctx, sid, 32);
    x25519(t->pub, t->priv, base);
    uint8_t *m = t->out + 5; int n = 0, hl = tw_slen(host); if (hl > 200) return -1;
    m[n++] = 1; n += 3;                                                           /* ClientHello, length patched below */
    m[n++] = 3; m[n++] = 3; pbytes(m, &n, random, 32); m[n++] = 32; pbytes(m, &n, sid, 32);
    p16(m, &n, 2); m[n++] = 0x13; m[n++] = 0x03; m[n++] = 1; m[n++] = 0;
    int extlen_at = n; n += 2;
    p16(m, &n, 0); p16(m, &n, hl + 5); p16(m, &n, hl + 3); m[n++] = 0; p16(m, &n, hl); pbytes(m, &n, (const uint8_t *)host, hl);     /* server_name */
    p16(m, &n, 10); p16(m, &n, 4); p16(m, &n, 2); p16(m, &n, 0x001d);                                                                  /* supported_groups */
    { static const uint8_t sa[18] = { 0x04,0x03, 0x08,0x04, 0x04,0x01, 0x05,0x03, 0x08,0x05, 0x05,0x01, 0x08,0x07, 0x08,0x06, 0x06,0x01 }; p16(m, &n, 13); p16(m, &n, 20); p16(m, &n, 18); pbytes(m, &n, sa, 18); }   /* signature_algorithms */
    p16(m, &n, 43); p16(m, &n, 3); m[n++] = 2; m[n++] = 3; m[n++] = 4;                                                                 /* supported_versions: 1.3 */
    p16(m, &n, 45); p16(m, &n, 2); m[n++] = 1; m[n++] = 1;                                                                             /* psk_key_exchange_modes */
    p16(m, &n, 51); p16(m, &n, 38); p16(m, &n, 36); p16(m, &n, 0x001d); p16(m, &n, 32); pbytes(m, &n, t->pub, 32);                     /* key_share */
    { int el = n - extlen_at - 2; m[extlen_at] = (uint8_t)(el >> 8); m[extlen_at + 1] = (uint8_t)el; }
    m[1] = (uint8_t)((n - 4) >> 16); m[2] = (uint8_t)((n - 4) >> 8); m[3] = (uint8_t)(n - 4);
    t->out[0] = 0x16; t->out[1] = 3; t->out[2] = 1; t->out[3] = (uint8_t)(n >> 8); t->out[4] = (uint8_t)n; t->out_len = 5 + n;
    sha256_init(&t->th); sha256_update(&t->th, m, n);
    t->state = TLS_WAIT_SH; t->rlen = 0; t->hdr_n = 0; t->msg_got = 0; t->msg_len = 0; t->s_encrypted = 0; t->cseq = t->sseq = 0; t->app_len = t->app_off = 0;
    return 0;
}
void tls_out_consumed(Tls *t, int n) { if (n >= t->out_len) { t->out_len = 0; return; } for (int i = 0; i + n < t->out_len; i++) t->out[i] = t->out[i + n]; t->out_len -= n; }

static int queue_record(Tls *t, uint8_t type, const uint8_t *d, int n) {         /* encrypt with the client traffic keys and append to out */
    int total = 5 + n + 1 + 16; if (t->out_len + total > TLS_OUT_MAX) return -1;
    uint8_t *r = t->out + t->out_len, nonce[12]; r[0] = 0x17; r[1] = 3; r[2] = 3; r[3] = (uint8_t)((n + 17) >> 8); r[4] = (uint8_t)(n + 17);
    tw_cpy(r + 5, d, n); r[5 + n] = type; nonce_for(t->civ, t->cseq++, nonce);
    aead_seal(t->ck, nonce, r, 5, r + 5, n + 1, r + 5);
    t->out_len += total; return 0;
}
int tls_send(Tls *t, const uint8_t *d, int n) { if (t->state != TLS_APP || n > 1400) return -1; return queue_record(t, 0x17, d, n); }
int tls_read(Tls *t, uint8_t *o, int max) { int k = t->app_len - t->app_off; if (k > max) k = max; if (k <= 0) return 0; tw_cpy(o, t->app + t->app_off, k); t->app_off += k; return k; }

static void handshake_done_server_finished(Tls *t) {
    uint8_t fk[32], want[32], th_after[32], emptyh[32]; Sha256 e;
    expand_label(t->s_hs, "finished", 0, 0, fk, 32); hmac_sha256(fk, 32, t->th_before, 32, 0, 0, want);
    uint8_t diff = 0; for (int i = 0; i < 32; i++) diff |= (uint8_t)(want[i] ^ t->fin[i]);
    if (diff) { set_err(t, -30); return; }
    sha_snapshot(&t->th, th_after);
    sha256_init(&e); sha256_final(&e, emptyh);
    { uint8_t derived[32], zero[32]; tw_zer(zero, 32); expand_label(t->hs_secret, "derived", emptyh, 32, derived, 32); hmac_sha256(derived, 32, zero, 32, 0, 0, t->master); }
    /* client Finished, still under the handshake traffic keys */
    { uint8_t cfk[32], vd[32], msg[36]; expand_label(t->c_hs, "finished", 0, 0, cfk, 32); hmac_sha256(cfk, 32, th_after, 32, 0, 0, vd);
      msg[0] = 20; msg[1] = 0; msg[2] = 0; msg[3] = 32; tw_cpy(msg + 4, vd, 32);
      static const uint8_t ccs[6] = { 0x14, 3, 3, 0, 1, 1 }; if (t->out_len + 6 > TLS_OUT_MAX) { set_err(t, -31); return; } tw_cpy(t->out + t->out_len, ccs, 6); t->out_len += 6;
      if (queue_record(t, 0x16, msg, 36) < 0) { set_err(t, -31); return; } }
    { uint8_t cap[32], sap[32]; expand_label(t->master, "c ap traffic", th_after, 32, cap, 32); expand_label(t->master, "s ap traffic", th_after, 32, sap, 32);
      traffic_keys(cap, t->ck, t->civ); t->cseq = 0; traffic_keys(sap, t->sk, t->siv); t->sseq = 0; }
    t->state = TLS_APP;
}
static void hs_feed(Tls *t, const uint8_t *d, int n) {                            /* handshake bytes (decrypted), streamed: only the Finished body is kept */
    while (n > 0 && t->state == TLS_WAIT_FIN) {
        if (t->hdr_n < 4) {
            t->hdr[t->hdr_n++] = *d++; n--;
            if (t->hdr_n == 4) { t->msg_type = t->hdr[0]; t->msg_len = (t->hdr[1] << 16) | (t->hdr[2] << 8) | t->hdr[3]; t->msg_got = 0;
                if (t->msg_type == 20) { if (t->msg_len != 32) { set_err(t, -32); return; } sha_snapshot(&t->th, t->th_before); }
                sha256_update(&t->th, t->hdr, 4); }
            if (t->hdr_n == 4 && t->msg_len == 0) { t->hdr_n = 0; }
            continue;
        }
        int k = t->msg_len - t->msg_got; if (k > n) k = n;
        sha256_update(&t->th, d, k);
        if (t->msg_type == 20) tw_cpy(t->fin + t->msg_got, d, k);
        t->msg_got += k; d += k; n -= k;
        if (t->msg_got == t->msg_len) {
            t->hdr_n = 0;
            if (t->msg_type == 20) { handshake_done_server_finished(t); if (t->state == TLS_APP && n > 0) { set_err(t, -33); } return; }
        }
    }
}
static void on_server_hello(Tls *t, const uint8_t *m, int len) {
    if (len < 4 + 2 + 32 + 1 || m[0] != 2) { set_err(t, -10); return; }
    int mlen = (m[1] << 16) | (m[2] << 8) | m[3]; if (mlen + 4 > len) { set_err(t, -10); return; }
    const uint8_t *p = m + 4, *end = m + 4 + mlen; p += 2;
    static const uint8_t hrr[32] = { 0xCF,0x21,0xAD,0x74,0xE5,0x9A,0x61,0x11,0xBE,0x1D,0x8C,0x02,0x1E,0x65,0xB8,0x91,0xC2,0xA2,0x11,0x16,0x7A,0xBB,0x8C,0x5E,0x07,0x9E,0x09,0xE2,0xC8,0xA8,0x33,0x9C };
    { int same = 1; for (int i = 0; i < 32; i++) if (p[i] != hrr[i]) same = 0; if (same) { set_err(t, -11); return; } }
    p += 32; int sl = *p++; p += sl; if (p + 2 + 1 + 2 > end) { set_err(t, -10); return; }
    if (p[0] != 0x13 || p[1] != 0x03) { set_err(t, -12); return; }                  /* the only suite we offered */
    p += 3; int el = (p[0] << 8) | p[1]; p += 2; const uint8_t *ee = p + el; if (ee > end) { set_err(t, -10); return; }
    const uint8_t *spub = 0; int ver13 = 0;
    while (p + 4 <= ee) { int et = (p[0] << 8) | p[1], elen = (p[2] << 8) | p[3]; p += 4; if (p + elen > ee) break;
        if (et == 43 && elen == 2 && p[0] == 3 && p[1] == 4) ver13 = 1;
        if (et == 51 && elen == 36 && p[0] == 0 && p[1] == 0x1d && p[2] == 0 && p[3] == 32) spub = p + 4;
        p += elen; }
    if (!ver13 || !spub) { set_err(t, -13); return; }
    sha256_update(&t->th, m, 4 + mlen);
    uint8_t shared[32], z = 0; x25519(shared, t->priv, spub); for (int i = 0; i < 32; i++) z |= shared[i]; if (!z) { set_err(t, -14); return; }
    uint8_t emptyh[32], derived[32], zero[32], hs_hash[32]; Sha256 e; tw_zer(zero, 32); sha256_init(&e); sha256_final(&e, emptyh);
    uint8_t early[32]; hmac_sha256(zero, 32, zero, 32, 0, 0, early);
    expand_label(early, "derived", emptyh, 32, derived, 32); hmac_sha256(derived, 32, shared, 32, 0, 0, t->hs_secret);
    sha_snapshot(&t->th, hs_hash);
    expand_label(t->hs_secret, "c hs traffic", hs_hash, 32, t->c_hs, 32); expand_label(t->hs_secret, "s hs traffic", hs_hash, 32, t->s_hs, 32);
    traffic_keys(t->c_hs, t->ck, t->civ); traffic_keys(t->s_hs, t->sk, t->siv); t->cseq = t->sseq = 0; t->s_encrypted = 1;
    t->state = TLS_WAIT_FIN; t->hdr_n = 0;
}
static void process_record(Tls *t) {
    uint8_t type = t->rec[0]; int rl = (t->rec[3] << 8) | t->rec[4];
    if (type == 0x14) return;                                                         /* change_cipher_spec (compatibility): ignore */
    if (type == 0x15 && !t->s_encrypted) { set_err(t, -20 - (rl > 1 ? t->rec[6] : 0)); return; }
    if (t->state == TLS_WAIT_SH) { if (type != 0x16) { set_err(t, -15); return; } on_server_hello(t, t->rec + 5, rl); return; }
    if (type != 0x17 || rl < 17) { set_err(t, -16); return; }
    uint8_t nonce[12]; nonce_for(t->siv, t->sseq++, nonce);
    if (aead_open(t->sk, nonce, t->rec, 5, t->rec + 5, rl - 16, t->rec + 5) < 0) { set_err(t, -17); return; }
    int pl = rl - 16; while (pl > 0 && t->rec[5 + pl - 1] == 0) pl--;
    if (pl == 0) { set_err(t, -18); return; }
    uint8_t inner = t->rec[5 + pl - 1]; pl--;
    if (inner == 0x16) { if (t->state == TLS_WAIT_FIN) hs_feed(t, t->rec + 5, pl); /* post-handshake tickets etc.: ignored */ }
    else if (inner == 0x17) { if (t->state == TLS_APP) { t->app = t->rec + 5; t->app_len = pl; t->app_off = 0; } else set_err(t, -19); }
    else if (inner == 0x15) { t->state = TLS_CLOSED; }                                /* close_notify (or a fatal alert): either way the stream ends */
}
int tls_want(Tls *t) {
    if (t->state == TLS_ERR || t->state == TLS_CLOSED || t->state == TLS_IDLE) return 0;
    if (t->app_off < t->app_len) return 0;
    if (t->rlen < 5) return 5 - t->rlen;
    return 5 + ((t->rec[3] << 8) | t->rec[4]) - t->rlen;
}
int tls_rx(Tls *t, const uint8_t *d, int n) {
    int used = 0;
    while (n > 0 && t->state != TLS_ERR) {
        int want = tls_want(t); if (want <= 0) break; int k = n < want ? n : want;
        tw_cpy(t->rec + t->rlen, d, k); t->rlen += k; d += k; n -= k; used += k;
        if (t->rlen == 5) { int rl = (t->rec[3] << 8) | t->rec[4]; if (rl > 16384 + 256 || rl == 0) { set_err(t, -40); break; } }
        if (t->rlen >= 5 && t->rlen == 5 + ((t->rec[3] << 8) | t->rec[4])) { process_record(t); t->rlen = 0; if (t->app_off < t->app_len) break; }
    }
    return used;
}
