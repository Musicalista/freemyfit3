/* App-level device under test: the Internet-mode web reader (webreader.inc.c + net.inc.c + netstack.c) with a FAKE Bluetooth layer, driven by net/app_sim.py.
 * In:  'F' frame from the phone, 'K' tick (u32 now_ms), 'G' go (url/search), 'T' tap (u16 x, u16 y), 'P' dump canvas to a PPM (path), 'Q' status.
 * Out: 'f' frame to the phone, 's' status (see below), 'z' end of reply. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static unsigned vnow = 1000;
static unsigned char fake_timer[64]; static int buzz;
static void *g_ptrs[64]; static int g_nptr;
static unsigned P2Uf(void *p) { for (int i = 0; i < g_nptr; i++) if (g_ptrs[i] == p) return (unsigned)(i + 1); g_ptrs[g_nptr++] = p; return (unsigned)g_nptr; }
static void *U2Pf(unsigned u) { return u && u <= (unsigned)g_nptr ? g_ptrs[u - 1] : 0; }
#define P2U(p) P2Uf((void *)(p))
#define U2P(u) U2Pf(u)
#define UI_LANG_ID (68u)
#define MOTOR_ONCE(a, b) (buzz++, 0)
#define MALLOC(n) malloc(n)
#define FREE(p) free(p)
#define TICK_GET() (vnow)
#define INVALIDATE(o) ((void)0)
#define ADD_FLAG(o, f) ((void)0)
#define CLEAR_FLAG(o, f) ((void)0)
#define IMG_CREATE(r) ((void *)1)
#define IMG_SET_SRC(i, s) ((void)0)
#define OBJ_ALIGN_TO(a, b, c, d, e) ((void)0)
#define ADD_EVENT_CB(o, cb, f, u) ((void *)0)
#define TIMER_DEL(t) ((void)0)
#define TIMER_CREATE(cb, p, u) (memcpy(fake_timer + 0xC, &(u), sizeof(void *)), (void *)fake_timer)
typedef struct { int code; void *user; } Ev;
#define EV_CODE(e) (((Ev *)(e))->code)
#define EV_USER(e) (((Ev *)(e))->user)
#define EV_TARGET(e) ((void *)1)
static int tp_down, tp_x, tp_y;
static int fake_tp(unsigned char *s) { s[1] = tp_down ? 1 : 0; *(uint16_t *)(s + 4) = (uint16_t)tp_x; *(uint16_t *)(s + 6) = (uint16_t)tp_y; return 0; }
#define TP_SAMPLE_GET(s) fake_tp(s)
#define SEND_REPLY(a, b, c) 0
/* fake Bluetooth */
static void out(char t, const uint8_t *d, int n);
static unsigned char fake_dev[2][0x124]; static unsigned char fake_chan[0x100]; static void *ev_cb, *data_cb; static int open_ev_pending;
static int fake_bt_call(unsigned p0, unsigned p1, unsigned p2, unsigned p3, void *fn) { ((void (*)(unsigned, unsigned, unsigned, unsigned))fn)(p0, p1, p2, p3); return 0; }
static unsigned fake_l2cap_open(const unsigned char *a, unsigned psm, unsigned mode, unsigned mtu, void *ev, void *dt) { (void)a; (void)mode; if (psm != 0x0F || mtu < 1691) return 0; ev_cb = ev; data_cb = dt; open_ev_pending = 1; return 0x1234u; }
static unsigned fake_l2cap_send(unsigned handle, const void *d, unsigned len, unsigned tag) { (void)handle; (void)tag; out('f', (const uint8_t *)d, (int)len); return 0; }
static void fake_l2cap_close(unsigned a, unsigned chan, unsigned reason) { (void)a; (void)chan; (void)reason; }
static unsigned char *fake_ppb_data(void *pb) { return (unsigned char *)pb + 0x10; }
#define BT_CALL(a, b, c, d, f) fake_bt_call(a, b, c, d, f)
#define L2CAP_OPEN(a, b, c, d, e, f) fake_l2cap_open(a, b, c, d, e, f)
#define L2CAP_SEND(a, b, c, d) fake_l2cap_send(a, b, c, d)
#define L2CAP_CLOSE(a, b, c) fake_l2cap_close(a, b, c)
#define PPB_DATA(pb) fake_ppb_data(pb)
#define BT_DEV(i) (fake_dev[i])
static void *g_wr;
#define NG_FIND_CTX() (g_wr)
#include "fit3_apps.c"

static void out(char t, const uint8_t *d, int n) { uint8_t h[3] = { (uint8_t)t, (uint8_t)(n >> 8), (uint8_t)n }; fwrite(h, 1, 3, stdout); if (n) fwrite(d, 1, (size_t)n, stdout); }
static void dump(const char *path) {
    WR *w = (WR *)g_wr; FILE *f = fopen(path, "wb"); if (!f) return; fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = w->px[i]; unsigned char c[3] = { (unsigned char)((p >> 11 & 31) * 255 / 31), (unsigned char)((p >> 5 & 63) * 255 / 63), (unsigned char)((p & 31) * 255 / 31) }; fwrite(c, 1, 3, f); }
    fclose(f);
}
int main(int argc, char **argv) {
    static uint8_t in[4000];
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
#endif
    memcpy(fake_dev[0], "\x11\x22\x33\x44\x55\x66", 6); fake_dev[0][6] = 1;           /* a connected phone in slot 0 */
    if (!(argc > 1 && argv[1][0] == 'a' ? ai_open((void *)1, 0) : argc > 1 && argv[1][0] == 'd' ? dial_open((void *)1, 0) : net_open((void *)1, 0))) return 2;
    g_wr = *(void **)(fake_timer + 0xC); WR *w = (WR *)g_wr;
    for (;;) {
        uint8_t h[3]; if (fread(h, 1, 3, stdin) != 3) break; int n = (h[1] << 8) | h[2]; if (n > (int)sizeof in - 1 || (n && fread(in, 1, (size_t)n, stdin) != (size_t)n)) break; in[n] = 0;
        switch (h[0]) {
        case 'F': { static uint8_t pb[0x10 + 1800]; memset(pb, 0, 0x10); *(uint16_t *)(pb + 8) = (uint16_t)n; memcpy(pb + 0x10, in, (size_t)n); ng_data(0, 0x1234, pb); break; }
        case 'K': vnow = ((unsigned)in[0] << 24) | ((unsigned)in[1] << 16) | ((unsigned)in[2] << 8) | in[3];
                  if (open_ev_pending && vnow > 1100) { open_ev_pending = 0; ((void (*)(void *, unsigned, unsigned, void *, unsigned))ev_cb)(0, 1, 0x1234, 0, 0); }
                  wr_tick(fake_timer); break;
        case 'G': wr_go(w, (const char *)in, 1); break;
        case 'T': { tp_x = (in[0] << 8) | in[1]; tp_y = (in[2] << 8) | in[3]; tp_down = 1; Ev e = { 1, w }; wr_event(&e); tp_down = 0; e.code = 8; wr_event(&e); e.code = 7; wr_event(&e); break; }
        case 'P': dump((const char *)in); break;
        case 'Q': { NG *g = (NG *)w->ng; uint8_t s[16 + 48 + 48]; memset(s, 0, sizeof s); s[0] = g->phase; s[1] = g->net->state; s[2] = g->net->sc.state; s[3] = (uint8_t)w->mode; s[4] = (uint8_t)w->nlinks; s[5] = (uint8_t)g->last_ev;
                   s[8] = g->ds; s[9] = (uint8_t)g->tls ? (uint8_t)((Tls *)g->tls)->state : 9; s[6] = (uint8_t)(w->nlines >> 8); s[7] = (uint8_t)w->nlines; strncpy((char *)s + 16, w->title, 47); strncpy((char *)s + 64, (const char *)w->buf + w->text_off, 47); out('s', s, 16 + 48 + 48); break; }
        default: break;
        }
        out('z', 0, 0); fflush(stdout);
    }
    return 0;
}
