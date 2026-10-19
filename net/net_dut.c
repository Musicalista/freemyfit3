/* Device-under-test driver: runs the real netstack.c as a host program, controlled over stdin/stdout by net/phone_sim.py (virtual time, deterministic).
 * Message = type(1) len(2, BE) payload.   In:  'F' frame from the phone, 'K' tick (u32 BE now_ms), 'U' link up, 'S' secure connect (port u16, psk[32], host...),
 *                                           'W' sc_send(payload), 'Q' status.
 * Out: 'f' frame to the phone, 'm' decrypted secure-channel message, 'e' secure-channel error, 'r' (1 byte) sc_send result, 's' status, 'z' end of reply. */
#include "netstack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static uint32_t vnow, rng = 0x12345678u;
static Net net;
static void out(char t, const uint8_t *d, int n) { uint8_t h[3] = { (uint8_t)t, (uint8_t)(n >> 8), (uint8_t)n }; fwrite(h, 1, 3, stdout); if (n) fwrite(d, 1, (size_t)n, stdout); }
static void op_tx(void *c, const uint8_t *s, int n) { (void)c; out('f', s, n); }
static uint32_t op_now(void *c) { (void)c; return vnow; }
static void op_rnd(void *c, uint8_t *o, int n) { (void)c; for (int i = 0; i < n; i++) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; o[i] = (uint8_t)rng; } }
static void poll_rx(void) {
    static uint8_t buf[SC_MSG_MAX + 8]; int r;
    while ((r = sc_recv(&net, buf, sizeof buf)) > 0) out('m', buf, r);
    if (r < 0) out('e', 0, 0);
}
int main(void) {
    static const uint8_t mac[6] = { 0x02, 0xaa, 0xbb, 0xcc, 0xdd, 0xee }; NetOps ops = { op_tx, op_now, op_rnd, 0 };
    static uint8_t in[4000];
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
#endif
    net_init(&net, &ops, mac);
    for (;;) {
        uint8_t h[3]; if (fread(h, 1, 3, stdin) != 3) break; int n = (h[1] << 8) | h[2]; if (n > (int)sizeof in || (n && fread(in, 1, (size_t)n, stdin) != (size_t)n)) break;
        switch (h[0]) {
        case 'F': net_link_rx(&net, in, n); break;
        case 'K': vnow = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | in[3]; net_tick(&net); break;
        case 'U': net_link_up(&net); break;
        case 'S': { uint16_t port = (uint16_t)((in[0] << 8) | in[1]); in[n] = 0; int r = sc_connect(&net, (const char *)in + 34, port, in + 2); out('r', (uint8_t *)&r, 1); break; }
        case 'W': { uint8_t r = (uint8_t)sc_send(&net, in, n); out('r', &r, 1); break; }
        case 'Q': { uint8_t s[16]; s[0] = net.state; s[1] = net.tcp.state; s[2] = net.sc.state; s[3] = 0; for (int i = 0; i < 4; i++) { s[4 + i] = (uint8_t)(net.ip >> (24 - 8 * i)); s[8 + i] = (uint8_t)(net.gw >> (24 - 8 * i)); s[12 + i] = (uint8_t)(net.dns >> (24 - 8 * i)); } out('s', s, 16); break; }
        default: break;
        }
        poll_rx(); out('z', 0, 0); fflush(stdout);
    }
    return 0;
}
