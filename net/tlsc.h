/* TLS 1.3 client (original code): TLS_CHACHA20_POLY1305_SHA256 + X25519 only, SNI, no session resumption.
 * WARNING: the server certificate is NOT verified (no CA store on the watch): the traffic is encrypted but a man in the middle could impersonate a site.
 * Portable C, no libc, no globals (state lives in Tls). The caller moves bytes between the TCP socket and tls_rx()/tls_out(). Needs chacha.c for the AEAD. */
#ifndef TLSC_H
#define TLSC_H
#include <stdint.h>

#define TLS_REC_MAX (5 + 16384 + 256)
#define TLS_OUT_MAX 2400

enum { TLS_IDLE, TLS_WAIT_SH, TLS_WAIT_FIN, TLS_APP, TLS_CLOSED, TLS_ERR };

typedef struct { uint32_t h[8]; uint64_t total; uint8_t buf[64]; int n; } Sha256;

typedef struct Tls {
    uint8_t state; int err;
    uint8_t priv[32], pub[32];
    Sha256 th;                                             /* transcript hash */
    uint8_t hs_secret[32], master[32], c_hs[32], s_hs[32];
    uint8_t ck[32], civ[12], sk[32], siv[12]; uint64_t cseq, sseq; uint8_t s_encrypted;
    uint8_t *rec; int rlen;                                /* record being assembled (malloc'd by tls_new) */
    uint8_t *app; int app_len, app_off;                    /* decrypted application bytes not yet read */
    uint8_t hdr[4]; int hdr_n, msg_type, msg_len, msg_got; uint8_t fin[32], th_before[32];
    uint8_t out[TLS_OUT_MAX]; int out_len;                 /* bytes to send over TCP */
    void (*rnd)(void *ctx, uint8_t *o, int n); void *rctx;
} Tls;

Tls *tls_new(void *(*alloc)(uint32_t), void (*release)(void *));                      /* allocates the struct and its record buffers; 0 on failure */
void tls_free(Tls *t, void (*release)(void *));
int  tls_start(Tls *t, const char *host, void (*rnd)(void *, uint8_t *, int), void *ctx);   /* queues the ClientHello in t->out; 0 ok */
int  tls_want(Tls *t);                                       /* how many bytes of the next TCP data tls_rx() can take now (0 = drain tls_read() first, or finished) */
int  tls_rx(Tls *t, const uint8_t *d, int n);                /* feed at most tls_want() bytes from TCP; returns the bytes used; errors show as t->state == TLS_ERR */
int  tls_send(Tls *t, const uint8_t *d, int n);              /* queue application data (state TLS_APP); n <= 1400; 0 ok */
int  tls_read(Tls *t, uint8_t *o, int max);                  /* decrypted application bytes (0 = none yet) */
void tls_out_consumed(Tls *t, int n);                        /* drop n bytes from the front of t->out after they were sent */

/* primitives (exposed for tests) */
void sha256_init(Sha256 *s); void sha256_update(Sha256 *s, const uint8_t *d, int n); void sha256_final(Sha256 *s, uint8_t out[32]);
void hmac_sha256(const uint8_t *key, int klen, const uint8_t *a, int alen, const uint8_t *b, int blen, uint8_t out[32]);
void x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]);
#endif
