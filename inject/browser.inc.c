/* ================= Remote browser (full Chrome on the PC, drawn on the watch) =================
 * The PC bridge runs a real headless Chrome and returns 256x340 16-colour frames (webbridge/browser.js).
 * Watch -> PC: /user/web_req.txt  "R<seq>\n<command>\n[<command>\n]"   commands: go <url|search>, click x y, scroll dy, type <text>, enter, back, fwd, reload, zoom +-1
 * PC -> watch: /user/br_frame.bin  192-byte header (seq, size, mode raw/RLE, palette, title, url) + payload. Uses wr_file_* (webreader.inc.c). */
#define BR_FRAME "/user/br_frame.bin"
#define BR_BUF 46592
#define BR_FY 30
#define BR_FH 340
typedef struct {
    u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img, *timer; int from_menu;
    u8 *buf; u32 seq, seq_wait, wait_t0, last_poll; int waiting, dirty, have, zoom;
    int pressed, x, y, sx, sy, moved; char title[44], url[108];
} BR;

static int br_read_n(const char *path, u8 *buf, int n) {
    int fd = FS_OPEN(path, "r"); if (fd < 0) return -1;
    int got = 0, r; while (got < n && (r = FS_READ(fd, buf + got, (u32)(n - got))) > 0) got += r;
    FS_CLOSE(fd); return got;
}
static u32 br_u32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static int br_decode(BR *b, const u8 *f, int n) {                          /* returns 1 and paints the frame area on success */
    if (n < 192 || f[0] != 'F' || f[1] != '1') return 0;
    int W = f[6] | (f[7] << 8), H = f[8] | (f[9] << 8), mode = f[10]; u32 len = br_u32(f + 12);
    if (W != 256 || H != BR_FH || (int)len > n - 192) return 0;
    u16 pal[16]; for (int i = 0; i < 16; i++) pal[i] = (u16)(f[16 + i * 2] | (f[17 + i * 2] << 8));
    const u8 *p = f + 192; u16 *d = b->px + BR_FY * KBD_W; int total = W * H, o = 0;
    if (mode == 0) { for (u32 i = 0; i < len && o < total; i++) { d[o++] = pal[p[i] >> 4]; if (o < total) d[o++] = pal[p[i] & 15]; } }
    else { for (u32 i = 0; i < len && o < total; i++) { int run = (p[i] >> 4) + 1; u16 c = pal[p[i] & 15]; while (run-- && o < total) d[o++] = c; } }
    if (o != total) return 0;
    int i = 0; while (i < 43 && f[48 + i]) { b->title[i] = (char)f[48 + i]; i++; } b->title[i] = 0;
    i = 0; while (i < 107 && f[88 + i]) { b->url[i] = (char)f[88 + i]; i++; } b->url[i] = 0;
    b->seq = br_u32(f + 2); b->zoom = f[11]; b->have = 1; return 1;
}
static void br_itoa(char *s, int *n, int v) { char t[12]; int k = 0; if (v < 0) { s[(*n)++] = '-'; v = -v; } do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v); while (k) s[(*n)++] = t[--k]; }
static void br_send(BR *b, const char *l1, int a, int bb, const char *l2) {   /* l1 = verb (+ optional text arg in l1 itself); a,bb = ints appended when >= -99999 */
    char req[260]; int n = 0; u32 seq = (TICK_GET() & 0x3fffffffu) | 1u; if (seq == b->seq || seq == b->seq_wait) seq += 2;
    req[n++] = 'R'; br_itoa(req, &n, (int)seq); req[n++] = '\n';
    for (int i = 0; l1[i] && n < 230; i++) req[n++] = l1[i];
    if (a > -99999) { req[n++] = ' '; br_itoa(req, &n, a); } if (bb > -99999) { req[n++] = ' '; br_itoa(req, &n, bb); }
    req[n++] = '\n';
    if (l2) { for (int i = 0; l2[i] && n < 255; i++) req[n++] = l2[i]; req[n++] = '\n'; }
    wr_file_write(WR_REQ, (const u8 *)req, n);
    b->seq_wait = seq; b->wait_t0 = TICK_GET(); b->last_poll = 0; b->waiting = 1; b->dirty = 1;
}
static void br_kbd_go(void *ctx, const char *text) {
    BR *b = (BR *)ctx; char l[200]; int n = 0; l[n++] = 'g'; l[n++] = 'o'; l[n++] = ' '; for (int i = 0; text[i] && n < 190; i++) l[n++] = text[i]; l[n] = 0; br_send(b, l, -99999, -99999, 0);
}
static void br_kbd_type(void *ctx, const char *text) {
    BR *b = (BR *)ctx; char l[200]; int n = 0; const char *h = "type "; while (*h) l[n++] = *h++; for (int i = 0; text[i] && n < 190; i++) l[n++] = text[i]; l[n] = 0; br_send(b, l, -99999, -99999, "enter");
}
static void br_draw_ui(BR *b) {
    u16 *g = b->px;
    rect(g, 0, 0, KBD_W, BR_FY, C(22, 26, 42)); rect(g, 0, BR_FY + BR_FH, KBD_W, KBD_H - BR_FY - BR_FH, C(22, 26, 42));
    static const struct { int x, w; const char *l; } top[6] = { { 2, 26, "<" }, { 30, 26, ">" }, { 58, 26, "R" }, { 86, 32, "Ir" }, { 122, 30, "A-" }, { 154, 30, "A+" } };
    for (int i = 0; i < 6; i++) { rect(g, top[i].x, 2, top[i].w, 26, i == 3 ? C(30, 120, 60) : C(40, 60, 110)); int len = i < 3 ? 1 : 2; ptext(g, top[i].x + top[i].w / 2 - len * 8, 8, top[i].l, 2, C(255, 255, 255)); }
    rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255));
    if (b->waiting) { int d = (int)((TICK_GET() / 300u) % 4u); for (int i = 0; i < 3; i++) rect(g, 190 + i * 8, 10, 6, 6, i < d ? C(255, 210, 90) : C(70, 76, 100)); }
    rect(g, 2, 374, 52, 26, C(40, 60, 110)); ptext(g, 17, 382, "Cima", 1, C(255, 255, 255));
    rect(g, 58, 374, 52, 26, C(40, 60, 110)); ptext(g, 69, 382, "Baixo", 1, C(255, 255, 255));
    rect(g, 114, 374, 52, 26, C(30, 120, 60)); ptext(g, 126, 382, "Tecla", 1, C(255, 255, 255));
    draw_wrapped(g, b->title[0] ? b->title : "Navegador", 172, 382, 1, 10, 1, 0, C(255, 210, 90), 0);
}
static void br_welcome(BR *b) {
    u16 *g = b->px; rect(g, 0, BR_FY, KBD_W, BR_FH, C(12, 14, 22));
    draw_wrapped(g, "Navegador completo\n\nUm Chrome de verdade roda no seu PC e mostra a p\xc3\xa1" "gina aqui.\n\n1. No PC, abra a pasta fit3-webbridge e execute start-bridge.bat\n2. No Chrome, abra http://127.0.0.1:8787, escolha o rel\xc3\xb3" "gio e ligue o modo navegador\n3. Toque em Ir e digite um endere\xc3\xa7o ou uma busca\n\nToque na p\xc3\xa1" "gina para clicar, arraste para rolar e use Teclado para digitar nos campos.", 8, BR_FY + 8, 1, 30, 24, 0, C(235, 235, 240), 0);
}
static void br_close(BR *b) { TIMER_DEL(b->timer); ADD_FLAG(b->img, 1); FREE(b->cvblk); FREE(b->buf); if (b->from_menu) menu_open(b->root); }
static void br_tap(BR *b, int x, int y) {
    if (y < BR_FY) {
        if (x >= 216) { br_close(b); return; }
        if (b->waiting) return;
        if (x >= 2 && x < 28) br_send(b, "back", -99999, -99999, 0);
        else if (x >= 30 && x < 56) br_send(b, "fwd", -99999, -99999, 0);
        else if (x >= 58 && x < 84) br_send(b, "reload", -99999, -99999, 0);
        else if (x >= 86 && x < 118) kbd_open_cb(b->root, "Ir para", "Digite um endereco (ex.: example.com) ou uma busca", (void *)br_kbd_go, b);
        else if (x >= 122 && x < 152) br_send(b, "zoom", -1, -99999, 0);
        else if (x >= 154 && x < 184) br_send(b, "zoom", 1, -99999, 0);
        return;
    }
    if (y >= BR_FY + BR_FH) {
        if (x >= 114 && x < 166) { kbd_open_cb(b->root, "Digitar na pagina", "OK digita o texto e aperta Enter", (void *)br_kbd_type, b); return; }
        if (b->waiting) return;
        if (x < 56) br_send(b, "scroll", -(BR_FH * 3 / 4), -99999, 0); else if (x < 112) br_send(b, "scroll", BR_FH * 3 / 4, -99999, 0);
        return;
    }
    if (!b->waiting) br_send(b, "click", x, y - BR_FY, 0);
}
static void br_event(void *e) {
    int code = EV_CODE(e), x, y; BR *b = (BR *)EV_USER(e);
    if (code == 1) { if (touch_read(&x, &y)) { b->pressed = 1; b->x = x; b->y = y; b->sx = x; b->sy = y; b->moved = 0; } return; }
    if (code == 2) { if (touch_read(&x, &y)) { b->x = x; b->y = y; int dy = y - b->sy, dx = x - b->sx; if (dy > 14 || dy < -14 || dx > 14 || dx < -14) b->moved = 1; } return; }
    if (code == 8 || code == 3) {
        b->pressed = 0;
        if (b->moved && b->sy >= BR_FY && b->sy < BR_FY + BR_FH && !b->waiting) { int dy = b->y - b->sy; if (dy > 14 || dy < -14) br_send(b, "scroll", -dy, -99999, 0); }
        return;
    }
    if (code == 7 && !b->moved) br_tap(b, b->x, b->y);
}
static void br_tick(void *timer) {
    BR *b = *(BR **)((u8 *)timer + 0xC); u32 now = TICK_GET();
    if (b->waiting) {
        if (now - b->last_poll >= 700u) {
            b->last_poll = now; u8 hdr[16]; int n = br_read_n(BR_FRAME, hdr, 16);
            if (n == 16 && hdr[0] == 'F' && br_u32(hdr + 2) == b->seq_wait) {
                int tot = 192 + (int)br_u32(hdr + 12);
                if (tot <= BR_BUF) { int m = br_read_n(BR_FRAME, b->buf, tot); if (m == tot && br_decode(b, b->buf, m)) { b->waiting = 0; MOTOR_ONCE(1, 0); } }
            }
            if (b->waiting && now - b->wait_t0 > 120000u) { b->waiting = 0; draw_wrapped(b->px, "Sem resposta da ponte (120 s). Confira o PC.", 8, BR_FY + 8, 1, 30, 4, 0, C(255, 120, 120), 0); }
        }
        b->dirty = 1;
    } else if (now - b->last_poll >= 1500u) {                                    /* idle poll: the PC may have pushed a frame (mouse/keyboard on the PC) */
        b->last_poll = now; u8 hdr[16]; int n = br_read_n(BR_FRAME, hdr, 16);
        if (n == 16 && hdr[0] == 'F' && br_u32(hdr + 2) != b->seq) { int tot = 192 + (int)br_u32(hdr + 12);
            if (tot <= BR_BUF) { int m = br_read_n(BR_FRAME, b->buf, tot); if (m == tot && br_decode(b, b->buf, m)) b->dirty = 1; } }
    }
    if (b->dirty) { b->dirty = 0; br_draw_ui(b); INVALIDATE(b->img); }
}
static int br_open(void *root, int from_menu) {
    BR *b = (BR *)MALLOC(sizeof(BR)); if (!b) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); u8 *buf = (u8 *)MALLOC(BR_BUF);
    if (!blk || !buf) { if (blk) FREE(blk); if (buf) FREE(buf); FREE(b); return 0; }
    for (unsigned i = 0; i < sizeof *b; i++) ((u8 *)b)[i] = 0;
    b->cvblk = blk; b->px = (u16 *)((u8 *)blk + 8); b->root = root; b->from_menu = from_menu; b->buf = buf; b->zoom = 1;
    rect(b->px, 0, 0, KBD_W, KBD_H, C(12, 14, 22));
    int n = br_read_n(BR_FRAME, buf, BR_BUF);                                   /* show the last frame if one is still there */
    if (!(n > 192 && br_decode(b, buf, n))) br_welcome(b);
    br_draw_ui(b);
    b->img = make_canvas(root, b->dsc, b->px, (void *)br_event, b);
    b->timer = TIMER_CREATE((void *)br_tick, 150, b);
    return 1;
}
