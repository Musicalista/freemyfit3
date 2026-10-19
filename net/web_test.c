/* PC test of the whole direct-browsing pipeline (tlsc + webc) against real sites: usage: web_test <url or search words> [dump.txt] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "tlsc.h"
#include "webc.h"
static void *al(uint32_t n) { return malloc(n); }
static void fr(void *p) { free(p); }
static void rnd(void *c, uint8_t *o, int n) { (void)c; for (int i = 0; i < n; i++) o[i] = (uint8_t)(rand() >> 3); }
static Web W;
static int fetch(const char *url) {                                        /* 1 page ready, 2 redirect, -1 error */
    strncpy(W.url, url, sizeof W.url - 1); if (web_parse_url(&W, url)) { puts("bad url"); return -1; }
    struct addrinfo hints, *res; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; char ps[8]; snprintf(ps, sizeof ps, "%d", W.port);
    if (getaddrinfo(W.host, ps, &hints, &res)) { puts("dns failed"); return -1; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0); if (connect(s, res->ai_addr, (int)res->ai_addrlen)) { puts("connect failed"); return -1; }
    Tls *t = 0; if (W.https) { t = tls_new(al, fr); tls_start(t, W.host, rnd, 0); }
    char req[700]; int rl = web_request(&W, req, sizeof req); int sent = 0, result = 0; web_reset_response(&W);
    uint8_t buf[4096], app[4096]; unsigned long t0 = GetTickCount(), bytes = 0;
    if (!W.https) { send(s, req, rl, 0); sent = 1; }
    for (;;) {
        if (t && t->out_len) { int n = send(s, (const char *)t->out, t->out_len, 0); if (n <= 0) break; tls_out_consumed(t, n); }
        if (t && t->state == TLS_APP && !sent) { tls_send(t, (const uint8_t *)req, rl); sent = 1; continue; }
        if (t && t->state == TLS_ERR) { printf("TLS error %d\n", t->err); result = -1; break; }
        if (t) { int k; while ((k = tls_read(t, app, sizeof app)) > 0) { bytes += k; result = web_feed(&W, app, k); if (result) goto done; } if (t->state == TLS_CLOSED && tls_want(t) == 0) { result = web_eof(&W); break; } int want = tls_want(t); if (want <= 0) continue; if (want > (int)sizeof buf) want = sizeof buf; int n = recv(s, (char *)buf, want, 0); if (n <= 0) { result = web_eof(&W); break; } tls_rx(t, buf, n); }
        else { int n = recv(s, (char *)buf, sizeof buf, 0); if (n <= 0) { result = web_eof(&W); break; } bytes += n; result = web_feed(&W, buf, n); if (result) break; }
    }
done:
    printf("  %s -> code %d, %lu body bytes in %lu ms, result %d\n", url, W.code, bytes, GetTickCount() - t0, result);
    closesocket(s); if (t) tls_free(t, fr); return result;
}
int main(int argc, char **argv) {
    WSADATA wd; WSAStartup(MAKEWORD(2, 2), &wd); char url[400];
    if (web_looks_like_url(argv[1])) { if (!web_has_scheme(argv[1])) snprintf(url, sizeof url, "https://%s", argv[1]); else snprintf(url, sizeof url, "%s", argv[1]); } else web_search_url(argv[1], url, sizeof url);
    int r = 0; for (int hop = 0; hop < 6; hop++) { r = fetch(url); if (r != 2) break; char nu[400]; if (web_resolve(W.url, W.loc, nu, sizeof nu) < 0) { puts("bad redirect"); break; } printf("  redirect -> %s\n", nu); strcpy(url, nu); }
    static char page[30000]; int n = web_page(&W, page, sizeof page, 1);
    printf("PAGE (%d bytes, %d links, title '%s')\n", n, W.nl, W.title);
    FILE *f = argc > 2 ? fopen(argv[2], "wb") : 0; if (f) { fwrite(page, 1, n, f); fclose(f); }
    printf("%.700s\n...\n", page); return r == 1 ? 0 : 1;
}
