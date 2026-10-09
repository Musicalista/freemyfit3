/* ================= Web reader (text browser) =================
 * The watch has no network stack, so a PC "bridge" (webbridge/) converts pages to a tiny text format and exchanges two files
 * with the watch over its Bluetooth serial file protocol:
 *   /user/web_req.txt   watch -> PC : "R<seq>\n<url or search>\n"
 *   /user/web_page.txt  PC -> watch : "W1\nS<seq>\nU<url>\nT<title>\n<body>\n\x01\nL<n>\t<url>\n..."
 * Uses the firmware's own VFS table in RAM at 0x200f73b8: [0]=open(path,mode) [4]=read [8]=write [0xc]=close. No globals. */
#ifndef FS_TABLE
#define FS_TABLE ((u32 *)0x200f73b8)
#endif
#ifndef FS_OPEN
#define FS_OPEN ((int (*)(const char *, const char *))FS_TABLE[0])
#endif
#ifndef FS_READ
#define FS_READ ((int (*)(int, void *, u32))FS_TABLE[1])
#endif
#ifndef FS_WRITE
#define FS_WRITE ((int (*)(int, const void *, u32))FS_TABLE[2])
#endif
#ifndef FS_CLOSE
#define FS_CLOSE ((int (*)(int))FS_TABLE[3])
#endif
#define WR_REQ "/user/web_req.txt"
#define WR_PAGE "/user/web_page.txt"
#define WR_BUF 24576
#define WR_MAXLINES 1400
#define WR_COLS 31
#define WR_LH 13
#define WR_TOP 30
#define WR_BOT 372
#define WR_ROWS ((WR_BOT - WR_TOP) / WR_LH)
#define WR_MAXLINKS 61
enum { WM_PAGE, WM_LINKS, WM_WAIT, WM_DIAL };

typedef struct {
    u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img, *timer; int from_menu;
    u8 *buf; int len, text_off, text_end, nlinks; int link_off[WR_MAXLINKS];
    u16 *lines; int nlines, top, ltop;
    int pressed, x, y, sx, sy, lasty, moved, acc, tap, dirty, mode, kbd_open;
    u32 seq_wait, wait_t0, last_poll, last_seq; char title[48], url[128]; char hist[6][128]; int nh;
    char dnum[24]; int dlen;                                                       /* dialer (netmode 3) */
    void *ng; int netmode;                                                         /* Internet mode (NET_APP): NG state from net.inc.c */
} WR;

static int wr_slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int wr_file_read(const char *path, u8 *buf, int max) {
    int fd = FS_OPEN(path, "r"); if (fd < 0) return -1;
    int n = 0, r; while (n < max && (r = FS_READ(fd, buf + n, (u32)(max - n))) > 0) n += r;
    FS_CLOSE(fd); return n;
}
static int wr_file_write(const char *path, const u8 *buf, int len) {
    int fd = FS_OPEN(path, "w"); if (fd < 0) return -1;
    int r = FS_WRITE(fd, buf, (u32)len); FS_CLOSE(fd); return r;
}
static int wr_u8len(u8 c) { return c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4; }

/* greedy word wrap into line start offsets (relative to the text start) */
static void wr_wrap(WR *w) {
    const u8 *t = w->buf + w->text_off; int n = w->text_end - w->text_off, i = 0, col = 0, ls = 0, sp = -1, spcol = 0, nl = 0;
    w->lines[nl++] = 0;
    while (i < n && nl < WR_MAXLINES - 2) {
        u8 c = t[i]; int adv = wr_u8len(c); if (i + adv > n) adv = n - i;
        if (c == '\n') { w->lines[nl++] = (u16)(i + 1); i++; col = 0; ls = i; sp = -1; continue; }
        if (col >= WR_COLS && c != ' ') {
            if (sp > ls) { w->lines[nl++] = (u16)sp; ls = sp; col -= spcol; sp = -1; }
            else { w->lines[nl++] = (u16)i; ls = i; col = 0; sp = -1; }
        }
        if (c == ' ') { sp = i + 1; spcol = col + 1; }
        col++; i += adv;
    }
    w->lines[nl++] = (u16)n; w->nlines = nl - 1; if (w->nlines < 1) w->nlines = 1;
}
/* parse the page file already in buf[0..len). returns 1 when complete and valid */
static int wr_parse(WR *w, u32 want_seq) {
    u8 *b = w->buf; int n = w->len; if (n < 12 || b[0] != 'W' || b[1] != '1' || b[2] != '\n' || b[3] != 'S') return 0;
    u32 seq = 0; int i = 4; while (i < n && b[i] >= '0' && b[i] <= '9') seq = seq * 10 + (u32)(b[i++] - '0');
    if (want_seq && seq != want_seq) return 0;
    int tr = -1; for (int k = n - 2; k >= 4; k--) if (b[k] == '\n' && b[k + 1] == 1) { tr = k; break; }
    if (tr < 0) return 0;                                                             /* not fully written yet */
    int p = i; while (p < tr && b[p] != '\n') p++; p++;                                 /* after S line */
    int ustart = p; while (p < tr && b[p] != '\n') p++; int uend = p; p++;            /* U line */
    int tstart = p; while (p < tr && b[p] != '\n') p++; int tend = p; p++;            /* T line */
    int ul = uend - ustart - 1; if (ul < 0) ul = 0; if (ul > 127) ul = 127; for (int k = 0; k < ul; k++) w->url[k] = (char)b[ustart + 1 + k]; w->url[ul] = 0;
    int tl = tend - tstart - 1; if (tl < 0) tl = 0; if (tl > 47) tl = 47; for (int k = 0; k < tl; k++) w->title[k] = (char)b[tstart + 1 + k]; w->title[tl] = 0;
    w->text_off = p > tr ? tr : p; w->text_end = tr; w->nlinks = 0;
    for (int k = 0; k < WR_MAXLINKS; k++) w->link_off[k] = 0;
    int q = tr + 3;                                                                    /* after "\n\x01\n" */
    while (q < n) {
        if (b[q] == 'L') {
            int num = 0, r = q + 1; while (r < n && b[r] >= '0' && b[r] <= '9') num = num * 10 + (b[r++] - '0');
            if (r < n && b[r] == '\t' && num > 0 && num < WR_MAXLINKS) { w->link_off[num] = r + 1; if (num > w->nlinks) w->nlinks = num; }
        }
        while (q < n && b[q] != '\n') q++;
        if (q < n) { b[q] = 0; q++; }                                                  /* NUL-terminate each link url in place */
    }
    wr_wrap(w); w->top = 0; w->ltop = 0; w->last_seq = seq; return 1;
}
static int wr_load_file(WR *w, u32 want_seq) {
    int n = wr_file_read(WR_PAGE, w->buf, WR_BUF - 1); if (n <= 0) return 0;
    w->len = n; w->buf[n] = 0; return wr_parse(w, want_seq);
}
static void wr_set_text(WR *w, const char *title, const char *body) {                   /* build an in-memory page (help / errors) */
    int n = 0; const char *h = "W1\nS0\nU\nT"; while (*h) w->buf[n++] = (u8)*h++;
    for (int i = 0; title[i] && n < 200; i++) w->buf[n++] = (u8)title[i]; w->buf[n++] = '\n';
    for (int i = 0; body[i] && n < WR_BUF - 16; i++) w->buf[n++] = (u8)body[i];
    w->buf[n++] = '\n'; w->buf[n++] = 1; w->buf[n++] = '\n'; w->len = n; w->buf[n] = 0; w->url[0] = 0; wr_parse(w, 0); w->mode = WM_PAGE; w->dirty = 1;
}
#ifdef NET_APP
#include "net.inc.c"
#endif
static void wr_go(WR *w, const char *q, int push) {
    if (!q || !q[0]) return;
    if (push && w->url[0] && w->nh < 6) { int k = 0; while (w->url[k] && k < 127) { w->hist[w->nh][k] = w->url[k]; k++; } w->hist[w->nh][k] = 0; w->nh++; }
#ifdef NET_APP
    if (w->netmode) { ng_request(w, q); return; }
#endif
    u32 seq = (TICK_GET() & 0x3fffffffu) | 1u; if (seq == w->last_seq) seq += 2;
    u8 req[200]; int n = 0; req[n++] = 'R'; char d[12]; int dl = 0; u32 v = seq; do { d[dl++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (dl) req[n++] = (u8)d[--dl]; req[n++] = '\n';
    req[n++] = 't'; req[n++] = 'e'; req[n++] = 'x'; req[n++] = 't'; req[n++] = ' ';
    for (int i = 0; q[i] && n < 190; i++) req[n++] = (u8)q[i]; req[n++] = '\n';
    wr_file_write(WR_REQ, req, n);
    w->seq_wait = seq; w->wait_t0 = TICK_GET(); w->last_poll = 0; w->mode = WM_WAIT; w->dirty = 1;
}
static void wr_kbd_done(void *ctx, const char *text) { WR *w = (WR *)ctx; w->kbd_open = 0; wr_go(w, text, 1); }

static void wr_color_line(u16 *g, int x, int y, const u8 *s, const u8 *e, uint16_t base) {
    int cx = x; const u8 *tokend = 0;
    for (const u8 *p = s; p < e; ) {
        int adv = wr_u8len(*p); uint32_t cp = *p;
        if (adv == 2) cp = ((uint32_t)(p[0] & 31) << 6) | (p[1] & 63); else if (adv == 3) cp = ((uint32_t)(p[0] & 15) << 12) | ((uint32_t)(p[1] & 63) << 6) | (p[2] & 63);
        if (*p == '[' && !tokend) { const u8 *q = p + 1; while (q < e && *q >= '0' && *q <= '9') q++; if (q > p + 1 && q < e && *q == ']') tokend = q; }
        put(g, cx, y, cp, 1, (tokend && p <= tokend) ? C(90, 200, 255) : base); if (tokend && p >= tokend) tokend = 0;
        cx += 8; p += adv;
    }
}
#ifdef NET_APP
static void wr_close(WR *w);
/* ---------------- dialer (netmode 3): a keypad; CALL sends 'D<number>' through the secure proxy and the phone (Fit3 Hub app) places the call ---------------- */
static const char dl_keys[12] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#' };
static int dl_in(WR *w, int x, int y, int bw, int bh) { return w->x >= x && w->x < x + bw && w->y >= y && w->y < y + bh; }
static void wr_dial_draw(WR *w) {
    u16 *g = w->px; rect(g, 0, 0, KBD_W, KBD_H, C(16, 20, 30));
    ptext(g, 8, 9, TR("Discador", "Dialer"), 1, C(255, 210, 90)); rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255));
    rrect(g, 8, 34, 240, 52, 10, C(8, 10, 16));
    { int n = w->dlen, st = n > 14 ? n - 14 : 0; ptext(g, 244 - (n - st) * 16, 48, w->dnum + st, 2, C(255, 255, 255)); if (!n) ctext(g, 128, 52, TR("digite o numero", "enter a number"), 1, C(100, 110, 140)); }
    for (int r = 0; r < 4; r++) for (int c = 0; c < 3; c++) {
        int x = 8 + c * 82, y = 96 + r * 62, on = w->pressed && dl_in(w, x, y, 78, 58); char s[2] = { dl_keys[r * 3 + c], 0 };
        rrect(g, x, y, 78, 58, 12, on ? C(250, 200, 80) : C(44, 50, 70)); ctext(g, x + 39, y + 11, s, 3, on ? C(40, 24, 0) : C(255, 255, 255));
    }
    rrect(g, 8, 346, 78, 48, 12, C(210, 140, 40)); ctext(g, 47, 358, "<", 3, C(40, 24, 0));
    rrect(g, 92, 346, 156, 48, 12, w->dlen ? C(70, 190, 120) : C(60, 90, 80)); ctext(g, 170, 358, TR("LIGAR", "CALL"), 2, C(10, 50, 20));
}
static void wr_dial_tap(WR *w, int x, int y) {
    (void)x; (void)y;
    if (w->x >= 216 && w->y < 28) { wr_close(w); return; }
    for (int r = 0; r < 4; r++) for (int c = 0; c < 3; c++) if (dl_in(w, 8 + c * 82, 96 + r * 62, 78, 58)) { if (w->dlen < 20) { w->dnum[w->dlen++] = dl_keys[r * 3 + c]; w->dnum[w->dlen] = 0; } w->dirty = 1; return; }
    if (dl_in(w, 8, 346, 78, 48)) { if (w->dlen) w->dnum[--w->dlen] = 0; w->dirty = 1; }
    else if (dl_in(w, 92, 346, 156, 48) && w->dlen) { MOTOR_ONCE(1, 0); ng_request(w, w->dnum); }
}
#endif
static void wr_draw(WR *w) {
#ifdef NET_APP
    if (w->mode == WM_DIAL) { wr_dial_draw(w); return; }
#endif
    u16 *g = w->px; rect(g, 0, 0, KBD_W, KBD_H, C(12, 14, 22));
    /* top bar */
    rect(g, 0, 0, KBD_W, 28, C(22, 26, 42));
    rect(g, 2, 2, 28, 24, C(40, 60, 110)); put(g, 8, 4, '<', 2, C(255, 255, 255));
    rect(g, 32, 2, 28, 24, w->mode == WM_LINKS ? C(200, 140, 30) : C(40, 60, 110)); put(g, 38, 4, 'L', 2, C(255, 255, 255));
    rect(g, 62, 2, 34, 24, C(30, 120, 60)); ptext(g, 64, 4, TR("Ir", "Go"), 2, C(255, 255, 255));
    draw_wrapped(g, w->mode == WM_WAIT ? (w->netmode ? (w->netmode == 2 ? "IA" : w->netmode == 3 ? TR("Discador", "Dialer") : "Internet") : TR("Aguardando a ponte", "Waiting for the bridge")) : (w->title[0] ? w->title : "Web"), 100, 8, 1, 14, 1, 0, C(255, 210, 90), 0);
    rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255));
#ifdef NET_APP
    if (w->mode == WM_WAIT && w->netmode) { ng_draw_status(w, g); } else
#endif
    if (w->mode == WM_WAIT) {
        draw_wrapped(g, TR("Pedido enviado ao PC.\n\nNo PC: rode  node server.js  e abra  http://127.0.0.1:8787  no Chrome, escolha o rel\xc3\xb3" "gio e ligue o modo navegador.\n\nToque em X para cancelar.", "Request sent to the PC.\n\nOn the PC: run  node server.js  and open  http://127.0.0.1:8787  in Chrome, choose the watch and turn on browser mode.\n\nTap X to cancel."), 8, 50, 1, 30, 20, 0, C(235, 235, 240), 0);
        int dots = (int)((TICK_GET() / 400u) % 4u); for (int i = 0; i < dots; i++) rect(g, 100 + i * 14, 200, 8, 8, C(255, 210, 90));
    } else if (w->mode == WM_LINKS) {
        for (int i = 0; i < 14; i++) {
            int n = w->ltop + i + 1; if (n > w->nlinks) break; int y = WR_TOP + i * 24; rect(g, 2, y, 252, 22, C(24, 28, 46));
            if (!w->link_off[n]) continue; mg_num(g, 6, y + 5, n, 1, C(90, 200, 255));
            const char *u = (const char *)w->buf + w->link_off[n]; int len = wr_slen(u); if (len > 28) len = 28; char tmp[32]; for (int k = 0; k < len; k++) tmp[k] = u[k]; tmp[len] = 0;
            draw_wrapped(g, tmp, 30, y + 5, 1, 28, 1, 0, C(235, 235, 240), 0);
        }
        if (!w->nlinks) draw_wrapped(g, TR("(sem links)", "(no links)"), 8, 50, 1, 20, 1, 0, C(150, 160, 190), 0);
    } else {
        const u8 *t = w->buf + w->text_off;
        for (int r = 0; r < WR_ROWS; r++) {
            int li = w->top + r; if (li >= w->nlines) break; const u8 *s = t + w->lines[li], *e = t + w->lines[li + 1];
            while (e > s && (e[-1] == '\n' || e[-1] == ' ')) e--;
            wr_color_line(g, 4, WR_TOP + r * WR_LH, s, e, (e - s >= 2 && s[0] == '#' && s[1] == ' ') ? C(255, 210, 90) : C(235, 235, 240));
        }
    }
    /* bottom bar */
    rect(g, 0, 374, KBD_W, 28, C(22, 26, 42));
    rect(g, 2, 376, 60, 24, C(40, 60, 110)); ptext(g, 18, 382, "PgUp", 1, C(255, 255, 255)); rect(g, 66, 376, 60, 24, C(40, 60, 110)); ptext(g, 78, 382, "PgDn", 1, C(255, 255, 255));
    if (w->mode == WM_PAGE) { mg_num(g, 140, 382, w->top + 1, 1, C(150, 160, 190)); ptext(g, 140 + (mg_numw(w->top + 1)) * 8, 382, "/", 1, C(150, 160, 190)); mg_num(g, 148 + mg_numw(w->top + 1) * 8, 382, w->nlines, 1, C(150, 160, 190)); }
}
static int wr_link_at(WR *w, int x, int y) {                                          /* "[n]" token under the finger, or 0 */
    int r = (y - WR_TOP) / WR_LH, li = w->top + r; if (y < WR_TOP || r >= WR_ROWS || li >= w->nlines) return 0;
    const u8 *t = w->buf + w->text_off, *s = t + w->lines[li], *e = t + w->lines[li + 1]; int col = (x - 4) / 8, c = 0;
    for (const u8 *p = s; p < e; c++) {
        if (*p == '[') { const u8 *q = p + 1; int num = 0; while (q < e && *q >= '0' && *q <= '9') { num = num * 10 + (*q - '0'); q++; }
            if (q > p + 1 && q < e && *q == ']') { int wdt = (int)(q - p) + 1; if (col >= c - 1 && col <= c + wdt) return num; } }
        p += wr_u8len(*p);
    }
    return 0;
}
static void wr_close(WR *w) {
    TIMER_DEL(w->timer); ADD_FLAG(w->img, 1);
#ifdef NET_APP
    if (w->netmode && w->ng) { NG *g = (NG *)w->ng; ng_stop(g); w->ng = 0; FREE(g->net); FREE(g); }   /* the Bluetooth thread may still run a job: they only use the channel pointer and a malloc'd buffer */
#endif
    FREE(w->cvblk); FREE(w->buf); FREE(w->lines); if (w->from_menu) menu_open(w->root);
}
static void wr_nav_link(WR *w, int n) { if (n > 0 && n < WR_MAXLINKS && w->link_off[n]) wr_go(w, (const char *)w->buf + w->link_off[n], 1); }
static void wr_tap(WR *w, int x, int y) {
#ifdef NET_APP
    if (w->mode == WM_DIAL) { wr_dial_tap(w, x, y); return; }
    if (w->netmode == 3 && w->mode == WM_PAGE) { if (x >= 216 && y < 28) { wr_close(w); return; } w->mode = WM_DIAL; w->dirty = 1; return; }   /* the call result: tap goes back to the keypad */
#endif
    if (w->mode == WM_WAIT) {
        if (x >= 216 && y < 28) { wr_close(w); return; }
#ifdef NET_APP
        if (w->netmode && w->ng && ((NG *)w->ng)->phase == NP_FAIL) { NG *g = (NG *)w->ng; ng_stop(g); g->t0 = TICK_GET() | 1u; w->dirty = 1; }   /* tap = try again */
#endif
        return;
    }
    if (y < 28) {
        if (x >= 216) { wr_close(w); return; }
        if (x < 30) { if (w->nh > 0) { w->nh--; wr_go(w, w->hist[w->nh], 0); } return; }
        if (x >= 32 && x < 60) { w->mode = (w->mode == WM_LINKS) ? WM_PAGE : WM_LINKS; w->dirty = 1; return; }
        if (x >= 62 && x < 96) { w->kbd_open = w->netmode == 2 ? kbd_open_cb(w->root, TR("Pergunta", "Question"), TR("Digite sua pergunta para a IA", "Type your question for the AI"), (void *)wr_kbd_done, w) : kbd_open_cb(w->root, TR("Ir para", "Go to"), TR("Digite um endereco (ex.: example.com) ou uma busca", "Type an address (e.g. example.com) or a search"), (void *)wr_kbd_done, w); return; }
        return;
    }
    if (y >= 374) { int page = w->mode == WM_LINKS ? 14 : WR_ROWS - 1; int *tp = w->mode == WM_LINKS ? &w->ltop : &w->top; int mx = (w->mode == WM_LINKS ? w->nlinks : w->nlines) - 1;
        if (x < 64) *tp -= page; else if (x < 128) *tp += page; if (*tp > mx - (w->mode == WM_LINKS ? 13 : WR_ROWS - 1)) *tp = mx - (w->mode == WM_LINKS ? 13 : WR_ROWS - 1); if (*tp < 0) *tp = 0; w->dirty = 1; return; }
    if (w->mode == WM_LINKS) { int n = w->ltop + (y - WR_TOP) / 24 + 1; if (n <= w->nlinks) { w->mode = WM_PAGE; wr_nav_link(w, n); } return; }
    wr_nav_link(w, wr_link_at(w, x, y));
}
static void wr_event(void *e) {
    int code = EV_CODE(e), x, y; WR *w = (WR *)EV_USER(e);
    if (code == 1) { if (touch_read(&x, &y)) { w->pressed = 1; w->x = x; w->y = y; w->sx = x; w->sy = y; w->lasty = y; w->moved = 0; w->acc = 0; } return; }
    if (code == 2) {
        if (!touch_read(&x, &y)) return; w->x = x; w->y = y;
        if (w->mode == WM_WAIT || w->mode == WM_DIAL) return;
        if (!w->moved && (y - w->sy > 10 || w->sy - y > 10)) w->moved = 1;
        if (w->moved) {
            w->acc += y - w->lasty; w->lasty = y; int step = w->mode == WM_LINKS ? 24 : WR_LH; int mv = w->acc / step;
            if (mv) { w->acc -= mv * step; int *tp = w->mode == WM_LINKS ? &w->ltop : &w->top; int mx = (w->mode == WM_LINKS ? w->nlinks : w->nlines) - 1 - (w->mode == WM_LINKS ? 13 : WR_ROWS - 1);
                *tp -= mv; if (*tp > mx) *tp = mx; if (*tp < 0) *tp = 0; w->dirty = 1; }
        }
        return;
    }
    if (code == 8 || code == 3) { w->pressed = 0; return; }
    if (code == 7 && !w->moved) wr_tap(w, w->x, w->y);
}
static void wr_tick(void *timer) {
    WR *w = *(WR **)((u8 *)timer + 0xC); u32 now = TICK_GET();
#ifdef NET_APP
    if (w->netmode) ng_tick(w); else
#endif
    if (w->mode == WM_WAIT) {
        if (now - w->last_poll >= 1000u) {
            w->last_poll = now;
            if (wr_load_file(w, w->seq_wait)) { w->mode = WM_PAGE; MOTOR_ONCE(1, 0); w->dirty = 1; }
            else if (now - w->wait_t0 > 90000u) { wr_set_text(w, TR("Sem resposta", "No answer"), TR("A ponte no PC nao respondeu em 90 s.\nConfira o servidor, o modo navegador e o Bluetooth, e tente de novo.", "The PC bridge did not answer in 90 s.\nCheck the server, browser mode and Bluetooth, then try again.")); }
        }
        w->dirty = 1;                                                                  /* animate the dots */
    }
    if (w->dirty) { w->dirty = 0; wr_draw(w); INVALIDATE(w->img); }
}
static int wr_open2(void *root, int from_menu, int netmode) {
    WR *w = (WR *)MALLOC(sizeof(WR)); if (!w) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); u8 *buf = (u8 *)MALLOC(WR_BUF); u16 *lines = (u16 *)MALLOC(WR_MAXLINES * 2);
    if (!blk || !buf || !lines) { if (blk) FREE(blk); if (buf) FREE(buf); if (lines) FREE(lines); FREE(w); return 0; }
    for (unsigned i = 0; i < sizeof *w; i++) ((u8 *)w)[i] = 0;
    w->cvblk = blk; w->px = (u16 *)((u8 *)blk + 8); w->root = root; w->from_menu = from_menu; w->buf = buf; w->lines = lines;
    if (netmode) { w->mode = WM_WAIT; }
    else if (!wr_load_file(w, 0))
        wr_set_text(w, TR("Navegador web", "Web browser"),
            TR("Este navegador mostra paginas como texto, pela ponte no PC.\n\n"
            "1. No PC, rode:  node server.js  (pasta webbridge) e abra http://127.0.0.1:8787 no Chrome.\n"
            "2. Escolha o rel\xc3\xb3" "gio, teste os arquivos e ligue o modo navegador.\n"
            "3. Aqui, toque em Ir, digite um endere\xc3\xa7o ou uma busca e confirme com OK.\n\n"
            "Toque em um [n] para abrir o link. Arraste para rolar. L lista os links; < volta.",
               "This browser shows pages as text, through the bridge on the PC.\n\n"
               "1. On the PC, run  node server.js  (webbridge folder) and open http://127.0.0.1:8787 in Chrome.\n"
               "2. Choose the watch, test the files and turn on browser mode.\n"
               "3. Here, tap Go, type an address or a search and confirm with OK.\n\n"
               "Tap a [n] to open the link. Drag to scroll. L lists the links; < goes back."));
#ifdef NET_APP
    if (netmode) {
        NG *g = (NG *)MALLOC(sizeof(NG)); Net *net = (Net *)MALLOC(sizeof(Net));
        if (!g || !net) { if (g) FREE(g); if (net) FREE(net); FREE(blk); FREE(buf); FREE(lines); FREE(w); return 0; }
        for (unsigned i = 0; i < sizeof *g; i++) ((u8 *)g)[i] = 0;
        g->net = net; g->rng = TICK_GET() ^ (u32)(void *)g ^ 0x9e3779b9u; g->phase = NP_IDLE; g->t0 = 0; w->ng = g; w->netmode = netmode;
    }
#endif
    if (!netmode) w->mode = WM_PAGE;
    wr_draw(w);
    w->img = make_canvas(root, w->dsc, w->px, (void *)wr_event, w);
    w->timer = TIMER_CREATE((void *)wr_tick, netmode ? 50 : 100, w);
    return 1;
}
static int wr_open(void *root, int from_menu) { return wr_open2(root, from_menu, 0); }
#ifdef NET_APP
static int net_open(void *root, int from_menu) { return wr_open2(root, from_menu, 4); }                  /* direct web access (no proxy) */
static int dial_open(void *root, int from_menu) { return wr_open2(root, from_menu, 3); }                  /* phone dialer through the proxy */
static int ai_open(void *root, int from_menu) { return wr_open2(root, from_menu, 2); }      /* Groq AI chat through the proxy */
#endif

