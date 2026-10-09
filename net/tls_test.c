/* PC test of tlsc.c against real servers: handshake + HTTPS GET / + the first bytes of the answer. usage: tls_test host [path]  (also self-tests SHA-256, HMAC and X25519) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "tlsc.h"
static void *al(uint32_t n) { return malloc(n); }
static void fr(void *p) { free(p); }
static void rnd(void *c, uint8_t *o, int n) { (void)c; for (int i = 0; i < n; i++) o[i] = (uint8_t)(rand() >> 3); }
static void hex(const uint8_t *d, int n) { for (int i = 0; i < n; i++) printf("%02x", d[i]); puts(""); }
static int selftest(void) {
    uint8_t h[32]; Sha256 s; sha256_init(&s); sha256_update(&s, (const uint8_t *)"abc", 3); sha256_final(&s, h);
    printf("sha256(abc) = "); hex(h, 32); printf("   want        ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n");
    uint8_t key[20]; memset(key, 0x0b, 20); hmac_sha256(key, 20, (const uint8_t *)"Hi There", 8, 0, 0, h);
    printf("hmac RFC4231#1 = "); hex(h, 32); printf("   want           b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7\n");
    uint8_t k[32] = { 0xa5,0x46,0xe3,0x6b,0xf0,0x52,0x7c,0x9d,0x3b,0x16,0x15,0x4b,0x82,0x46,0x5e,0xdd,0x62,0x14,0x4c,0x0a,0xc1,0xfc,0x5a,0x18,0x50,0x6a,0x22,0x44,0xba,0x44,0x9a,0xc4 };
    uint8_t u[32] = { 0xe6,0xdb,0x68,0x67,0x58,0x30,0x30,0xdb,0x35,0x94,0xc1,0xa4,0x24,0xb1,0x5f,0x7c,0x72,0x66,0x24,0xec,0x26,0xb3,0x35,0x3b,0x10,0xa9,0x03,0xa6,0xd0,0xab,0x1c,0x4c }, o[32];
    x25519(o, k, u); printf("x25519 RFC7748 = "); hex(o, 32); printf("   want           c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552\n");
    return 0;
}
int main(int argc, char **argv) {
    selftest(); if (argc < 2) return 0;
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); const char *host = argv[1], *path = argc > 2 ? argv[2] : "/";
    struct addrinfo hints, *res; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "443", &hints, &res)) { puts("dns failed"); return 1; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0); if (connect(s, res->ai_addr, (int)res->ai_addrlen)) { puts("connect failed"); return 1; }
    Tls *t = tls_new(al, fr); if (!t || tls_start(t, host, rnd, 0)) { puts("tls start failed"); return 1; }
    int sent_req = 0, total = 0; char req[512]; snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Fit3\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n", path, host);
    unsigned long t0 = GetTickCount(); unsigned long hs_done = 0; uint8_t buf[4096], app[4096]; int first = 1;
    for (;;) {
        if (t->out_len) { int n = send(s, (const char *)t->out, t->out_len, 0); if (n <= 0) { puts("send failed"); break; } tls_out_consumed(t, n); }
        if (t->state == TLS_APP && !sent_req) { hs_done = GetTickCount() - t0; printf("handshake OK in %lu ms\n", hs_done); tls_send(t, (const uint8_t *)req, (int)strlen(req)); sent_req = 1; continue; }
        if (t->state == TLS_ERR) { printf("TLS ERROR %d\n", t->err); return 1; }
        int k; while ((k = tls_read(t, app, sizeof app)) > 0) { total += k; if (first) { first = 0; int e = 0; while (e < k && app[e] != '\n') e++; printf("answer: %.*s\n", e, app); } }
        if (t->state == TLS_CLOSED && tls_want(t) == 0) break;
        int want = tls_want(t); if (want <= 0) continue;
        if (want > (int)sizeof buf) want = (int)sizeof buf;
        int n = recv(s, (char *)buf, want, 0); if (n <= 0) break; tls_rx(t, buf, n);
    }
    printf("received %d bytes of application data, state %d\n", total, t->state);
    return 0;
}
