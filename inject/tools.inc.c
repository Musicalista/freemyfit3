/* ================= Utilities (original code): one launcher tile with six small apps =================
 * Calculator, Stopwatch + Timer, Flashlight (coloured screen), Notes (typed with the T9 keyboard, saved in /user/notes.txt), Counter, Dice + coin.
 * One canvas, one LVGL timer, one touch handler: the "kind" field says which screen is showing; X goes back to the grid, X on the grid back to Apps extras.
 * Floats only (the target has no software double support). Needs common.inc.c (mg_num) and kbd_open_cb. */
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
#define TL_NOTES "/user/notes.txt"
#define TL_BUF 2048
#define TL_ROWS 8
enum { TK_GRID, TK_CALC, TK_CHRONO, TK_LIGHT, TK_NOTES, TK_COUNT, TK_DICE };
typedef struct {
    u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img, *timer; int from_menu, kind;
    int pressed, x, y, tap, dirty, kbd;
    char entry[16]; int elen, fresh, err; float acc; char op;                                  /* calculator */
    int tab, run, trun, tring, nlaps; u32 t0, base, tset, tend, tleft, last_s, laps[6];       /* stopwatch / timer (ms) */
    int nsel, ntop, nlen; char nbuf[TL_BUF];                                                   /* notes */
    int cnt, lcol, dmode, dres, dres2; u32 rng;                                                /* counter, flashlight, dice */
} TL;

static int menu_open(void *root);
static void tl_x(u16 *g) { rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255)); }
static void tl_btn(u16 *g, int x, int y, int w, int h, const char *s, int sc, u16 bg, u16 fg) { rrect(g, x, y, w, h, 10, bg); ctext(g, x + w / 2, y + (h - 12 * sc) / 2, s, sc, fg); }
static int tl_in(TL *t, int x, int y, int w, int h) { return t->x >= x && t->x < x + w && t->y >= y && t->y < y + h; }

/* ---------------- grid ---------------- */
static const struct { const char *pt, *en; uint16_t bg, fg; } tl_tiles[6] = {
    { "Calculadora", "Calculator", C(176, 212, 246), C(40, 90, 170) }, { "Cronometro", "Stopwatch", C(176, 228, 200), C(40, 120, 84) }, { "Lanterna", "Flashlight", C(252, 238, 168), C(146, 120, 18) },
    { "Notas", "Notes", C(248, 190, 202), C(184, 56, 88) }, { "Contador", "Counter", C(208, 190, 242), C(106, 66, 168) }, { "Dados", "Dice", C(252, 212, 176), C(186, 106, 36) } };
static void tl_icon(u16 *g, int i, int cx, int cy, uint16_t fg, uint16_t bg) {
    switch (i) {
    case 0: for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) rrect(g, cx - 21 + c * 15, cy - 21 + r * 15, 12, 12, 3, fg); break;
    case 1: disc(g, cx, cy + 2, 21, fg); disc(g, cx, cy + 2, 16, bg); rect(g, cx - 1, cy - 11, 3, 13, fg); rect(g, cx, cy + 1, 10, 3, fg); rect(g, cx - 5, cy - 26, 10, 4, fg); break;
    case 2: disc(g, cx, cy - 6, 15, fg); rect(g, cx - 8, cy + 6, 16, 6, fg); rect(g, cx - 6, cy + 14, 12, 4, fg); rect(g, cx - 2, cy + 18, 4, 3, fg); break;
    case 3: rrect(g, cx - 16, cy - 22, 32, 44, 5, fg); for (int k = 0; k < 4; k++) rect(g, cx - 10, cy - 14 + k * 9, 20, 3, bg); break;
    case 4: ctext(g, cx, cy - 24, "+1", 4, fg); break;
    default: rrect(g, cx - 21, cy - 21, 42, 42, 9, fg); disc(g, cx - 10, cy - 10, 4, bg); disc(g, cx + 10, cy + 10, 4, bg); disc(g, cx, cy, 4, bg); disc(g, cx + 10, cy - 10, 4, bg); disc(g, cx - 10, cy + 10, 4, bg); break;
    }
}
static void tl_tile_rect(int i, int *x, int *y, int *w, int *h) { *x = 10 + (i % 2) * 120; *y = 50 + (i / 2) * 102; *w = 116; *h = 94; }
static void tl_grid_draw(TL *t) {
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, P_BG); ptext(g, 14, 8, TR("Utilitarios", "Tools"), 2, P_TEXT); tl_x(g);
    for (int i = 0; i < 6; i++) {
        int x, y, w, h; tl_tile_rect(i, &x, &y, &w, &h); card(g, x, y, w, h, 28, tl_tiles[i].bg); tl_icon(g, i, x + w / 2, y + 36, tl_tiles[i].fg, tl_tiles[i].bg);
        { const char *lb = TR(tl_tiles[i].pt, tl_tiles[i].en); int ln = 0; while (lb[ln]) ln++; if (ln > 7) ctext(g, x + w / 2, y + h - 22, lb, 1, tl_tiles[i].fg); else ctext(g, x + w / 2, y + h - 28, lb, 2, tl_tiles[i].fg); }
    }
}

/* ---------------- calculator ---------------- */
static float tl_parse(const char *s) {
    float v = 0; int i = 0, neg = 0; if (s[i] == '-') { neg = 1; i++; }
    while (s[i] >= '0' && s[i] <= '9') { v = v * 10.0f + (float)(s[i] - '0'); i++; }
    if (s[i] == '.') { i++; float f = 0.1f; while (s[i] >= '0' && s[i] <= '9') { v += (float)(s[i] - '0') * f; f *= 0.1f; i++; } }
    return neg ? -v : v;
}
static int tl_fmt(float v, char *o) {                                                          /* 0 ok, 1 error */
    int n = 0;
    if (v != v || v > 999999999.0f || v < -999999999.0f) { o[0] = 'E'; o[1] = 'R'; o[2] = 'R'; o[3] = 0; return 1; }
    if (v < 0) { o[n++] = '-'; v = -v; }
    u32 ip = (u32)v; int dg = 1; for (u32 q = ip; q >= 10; q /= 10) dg++;
    int k = 9 - dg; if (k > 6) k = 6; if (k < 0) k = 0; u32 sc = 1; for (int i = 0; i < k; i++) sc *= 10;
    u32 f = (u32)((v - (float)ip) * (float)sc + 0.5f); if (f >= sc) { ip++; f -= sc; }
    char d[12]; int m = 0; do { d[m++] = (char)('0' + ip % 10); ip /= 10; } while (ip); while (m) o[n++] = d[--m];
    if (k > 0 && f) { char fr[8]; for (int i = k - 1; i >= 0; i--) { fr[i] = (char)('0' + f % 10); f /= 10; } int e = k; while (e > 0 && fr[e - 1] == '0') e--; o[n++] = '.'; for (int i = 0; i < e; i++) o[n++] = fr[i]; }
    o[n] = 0; return 0;
}
static void tl_calc_reset(TL *t) { t->entry[0] = '0'; t->entry[1] = 0; t->elen = 1; t->fresh = 1; t->err = 0; t->acc = 0; t->op = 0; }
static void tl_calc_commit(TL *t) {
    float v = tl_parse(t->entry);
    if (t->op && !t->fresh) {
        float a = t->acc, r = 0;
        switch (t->op) { case '+': r = a + v; break; case '-': r = a - v; break; case '*': r = a * v; break; default: if (v == 0.0f) { t->err = 1; } else r = a / v; break; }
        if (!t->err) { t->acc = r; t->err = tl_fmt(r, t->entry); t->elen = 0; while (t->entry[t->elen]) t->elen++; } else { t->entry[0] = 'E'; t->entry[1] = 'R'; t->entry[2] = 'R'; t->entry[3] = 0; t->elen = 3; }
    } else if (!t->op) t->acc = v;
}
static void tl_calc_key(TL *t, char k) {
    if (k == 'C') { tl_calc_reset(t); return; }
    if (t->err) return;
    if ((k >= '0' && k <= '9') || k == '.') {
        if (t->fresh) { t->elen = 0; t->entry[0] = 0; t->fresh = 0; if (k == '.') { t->entry[0] = '0'; t->elen = 1; } }
        if (k == '.') { for (int i = 0; i < t->elen; i++) if (t->entry[i] == '.') return; }
        if (t->elen == 1 && t->entry[0] == '0' && k != '.') t->elen = 0;
        if (t->elen < 9) { t->entry[t->elen++] = k; t->entry[t->elen] = 0; }
    } else if (k == '<') {
        if (t->fresh) return; if (t->elen > 0) t->elen--; t->entry[t->elen] = 0; if (t->elen == 0 || (t->elen == 1 && t->entry[0] == '-')) { t->entry[0] = '0'; t->entry[1] = 0; t->elen = 1; }
    } else if (k == 'S') {
        if (t->entry[0] == '-') { for (int i = 0; i < t->elen; i++) t->entry[i] = t->entry[i + 1]; t->elen--; }
        else if (!(t->elen == 1 && t->entry[0] == '0') && t->elen < 9) { for (int i = t->elen; i >= 0; i--) t->entry[i + 1] = t->entry[i]; t->entry[0] = '-'; t->elen++; }
    } else if (k == '%') { t->err = tl_fmt(tl_parse(t->entry) / 100.0f, t->entry); t->elen = 0; while (t->entry[t->elen]) t->elen++; }
    else { tl_calc_commit(t); if (t->err) return; t->op = (k == '=') ? 0 : k; t->fresh = 1; }
}
static const char tl_ckeys[20] = { 'C', 'S', '%', '/', '7', '8', '9', '*', '4', '5', '6', '-', '1', '2', '3', '+', '0', '.', '<', '=' };
static void tl_calc_draw(TL *t) {
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, C(20, 22, 32)); tl_x(g); ptext(g, 8, 8, TR("Calculadora", "Calculator"), 1, C(160, 170, 200));
    rrect(g, 6, 34, 244, 62, 10, C(10, 12, 18)); { int sc = t->elen > 6 ? 2 : 3, w = t->elen * 8 * sc; ptext(g, 244 - w, 34 + (62 - 12 * sc) / 2, t->entry, sc, t->err ? C(255, 90, 90) : C(255, 255, 255)); }
    if (t->op) { char o[2] = { t->op == '*' ? 'x' : t->op, 0 }; ptext(g, 12, 38, o, 1, C(250, 190, 60)); }
    for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) {
        char k = tl_ckeys[r * 4 + c]; int op = c == 3, fn = r == 0 && c < 3; char s[3] = { k == 'S' ? '~' : k == '*' ? 'x' : k, 0, 0 }; if (k == 'S') { s[0] = '+'; s[1] = '-'; }
        int x = 6 + c * 62, y = 106 + r * 58, on = t->pressed && tl_in(t, x, y, 58, 54);
        tl_btn(g, x, y, 58, 54, s, 2, on ? C(255, 255, 255) : op ? C(240, 160, 40) : fn ? C(90, 96, 120) : C(48, 52, 72), op && !on ? C(40, 24, 0) : on ? C(20, 20, 30) : C(255, 255, 255));
    }
}
static void tl_calc_tap(TL *t) { for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) if (tl_in(t, 6 + c * 62, 106 + r * 58, 58, 54)) { tl_calc_key(t, tl_ckeys[r * 4 + c]); t->dirty = 1; return; } }

/* ---------------- stopwatch + timer ---------------- */
static void tl_time(u32 ms, char *o, int cs) {                                                  /* MM:SS.cc or MM:SS */
    u32 s = ms / 1000, m = s / 60; s %= 60; if (m > 99) m = 99; o[0] = (char)('0' + m / 10); o[1] = (char)('0' + m % 10); o[2] = ':'; o[3] = (char)('0' + s / 10); o[4] = (char)('0' + s % 10);
    if (cs) { u32 c = (ms % 1000) / 10; o[5] = '.'; o[6] = (char)('0' + c / 10); o[7] = (char)('0' + c % 10); o[8] = 0; } else o[5] = 0;
}
static u32 tl_sw_ms(TL *t) { return t->run ? t->base + (TICK_GET() - t->t0) : t->base; }
static u32 tl_tm_ms(TL *t) { if (!t->trun) return t->tleft; u32 now = TICK_GET(); return (int)(t->tend - now) > 0 ? t->tend - now : 0; }
static void tl_chrono_draw(TL *t) {
    u16 *g = t->px; char b[10]; rect(g, 0, 0, KBD_W, KBD_H, C(18, 24, 30)); tl_x(g);
    tl_btn(g, 10, 34, 114, 30, TR("CRONOMETRO", "STOPWATCH"), 1, t->tab == 0 ? C(80, 170, 120) : C(44, 52, 66), C(255, 255, 255)); tl_btn(g, 132, 34, 114, 30, "TIMER", 1, t->tab == 1 ? C(80, 170, 120) : C(44, 52, 66), C(255, 255, 255));
    if (t->tab == 0) {
        tl_time(tl_sw_ms(t), b, 1); ctext(g, 128, 84, b, 4, C(255, 255, 255));
        tl_btn(g, 10, 150, 114, 56, t->run ? TR("PARAR", "STOP") : TR("INICIAR", "START"), 2, t->run ? C(210, 80, 80) : C(70, 190, 120), C(255, 255, 255));
        tl_btn(g, 132, 150, 114, 56, t->run ? TR("VOLTA", "LAP") : TR("ZERAR", "RESET"), 2, C(60, 66, 90), C(255, 255, 255));
        for (int i = 0; i < t->nlaps; i++) { char nm[4] = { (char)('1' + i), ':', 0, 0 }; tl_time(t->laps[i], b, 1); ptext(g, 40, 226 + i * 24, nm, 2, C(150, 160, 190)); ptext(g, 80, 226 + i * 24, b, 2, C(255, 255, 255)); }
    } else {
        tl_time(tl_tm_ms(t) + (t->trun ? 999 : 0), b, 0); ctext(g, 128, 84, b, 5, t->tring ? C(255, 120, 120) : C(255, 255, 255));
        tl_btn(g, 10, 140, 74, 44, "-1m", 2, C(60, 66, 90), C(255, 255, 255)); tl_btn(g, 90, 140, 74, 44, "+1m", 2, C(60, 66, 90), C(255, 255, 255)); tl_btn(g, 170, 140, 76, 44, "+10s", 2, C(60, 66, 90), C(255, 255, 255));
        tl_btn(g, 10, 196, 114, 56, t->trun ? TR("PAUSAR", "PAUSE") : TR("INICIAR", "START"), 2, t->trun ? C(210, 150, 60) : C(70, 190, 120), C(255, 255, 255)); tl_btn(g, 132, 196, 114, 56, TR("ZERAR", "RESET"), 2, C(60, 66, 90), C(255, 255, 255));
        if (t->tring) ctext(g, 128, 280, TR("TEMPO ACABOU!", "TIME IS UP!"), 2, C(255, 120, 120));
    }
}
static void tl_chrono_tap(TL *t) {
    if (tl_in(t, 10, 34, 114, 30)) { t->tab = 0; t->dirty = 1; return; } if (tl_in(t, 132, 34, 114, 30)) { t->tab = 1; t->dirty = 1; return; }
    if (t->tab == 0) {
        if (tl_in(t, 10, 150, 114, 56)) { if (t->run) { t->base += TICK_GET() - t->t0; t->run = 0; } else { t->t0 = TICK_GET(); t->run = 1; } }
        else if (tl_in(t, 132, 150, 114, 56)) { if (t->run) { if (t->nlaps < 6) t->laps[t->nlaps++] = tl_sw_ms(t); } else { t->base = 0; t->nlaps = 0; } }
    } else {
        if (tl_in(t, 10, 140, 74, 44) && !t->trun) { t->tset = t->tset > 60000 ? t->tset - 60000 : 0; t->tleft = t->tset; }
        else if (tl_in(t, 90, 140, 74, 44) && !t->trun) { t->tset += 60000; if (t->tset > 5999000) t->tset = 5999000; t->tleft = t->tset; }
        else if (tl_in(t, 170, 140, 76, 44) && !t->trun) { t->tset += 10000; if (t->tset > 5999000) t->tset = 5999000; t->tleft = t->tset; }
        else if (tl_in(t, 10, 196, 114, 56)) { t->tring = 0; if (t->trun) { t->tleft = tl_tm_ms(t); t->trun = 0; } else if (t->tleft) { t->tend = TICK_GET() + t->tleft; t->trun = 1; } }
        else if (tl_in(t, 132, 196, 114, 56)) { t->trun = 0; t->tring = 0; t->tleft = t->tset; }
    }
    t->dirty = 1;
}
static void tl_chrono_tick(TL *t) {
    if (t->trun && tl_tm_ms(t) == 0) { t->trun = 0; t->tleft = 0; t->tring = 10; t->last_s = 0; t->dirty = 1; }
    if (t->tring && TICK_GET() - t->last_s >= 1000) { t->last_s = TICK_GET(); MOTOR_ONCE(4, 0); if (--t->tring == 0) { t->tleft = t->tset; } t->dirty = 1; }
    if (t->run || t->trun) t->dirty = 1;
}

/* ---------------- flashlight ---------------- */
static void tl_light_draw(TL *t) {
    static const uint16_t col[5] = { C(255, 255, 255), C(255, 214, 150), C(255, 40, 40), C(60, 255, 90), C(80, 150, 255) };
    rect(t->px, 0, 0, KBD_W, KBD_H, col[t->lcol % 5]); tl_x(t->px);
}

/* ---------------- notes ---------------- */
static int tl_nlines(TL *t) { int n = 0; for (int i = 0; i < t->nlen; i++) if (t->nbuf[i] == '\n') n++; return n; }
static int tl_nline(TL *t, int idx, int *len) { int s = 0, k = 0; for (int i = 0; i < t->nlen; i++) if (t->nbuf[i] == '\n') { if (k == idx) { *len = i - s; return s; } k++; s = i + 1; } *len = 0; return 0; }
static void tl_nsave(TL *t) { int fd = FS_OPEN(TL_NOTES, "w"); if (fd < 0) return; if (t->nlen) FS_WRITE(fd, t->nbuf, (u32)t->nlen); FS_CLOSE(fd); }
static void tl_nload(TL *t) {
    t->nlen = 0; int fd = FS_OPEN(TL_NOTES, "r"); if (fd < 0) return; int n = 0, r;
    while (n < TL_BUF - 1 && (r = FS_READ(fd, t->nbuf + n, (u32)(TL_BUF - 1 - n))) > 0) n += r; FS_CLOSE(fd);
    while (n > 0 && t->nbuf[n - 1] != '\n') n--;                                                    /* keep whole lines only */
    t->nlen = n;
}
static void tl_note_done(void *ctx, const char *text) {
    TL *t = (TL *)ctx; t->kbd = 0; int n = 0; while (text[n] && n < 60) n++; if (!n || t->nlen + n + 1 >= TL_BUF) { t->dirty = 1; return; }
    for (int i = 0; i < n; i++) t->nbuf[t->nlen++] = text[i] == '\n' ? ' ' : text[i];
    t->nbuf[t->nlen++] = '\n'; t->nsel = tl_nlines(t) - 1; tl_nsave(t); t->dirty = 1;
}
static void tl_notes_draw(TL *t) {
    u16 *g = t->px; int nl = tl_nlines(t); rect(g, 0, 0, KBD_W, KBD_H, C(250, 244, 230)); rect(g, 0, 0, KBD_W, 30, C(184, 56, 88)); ptext(g, 8, 9, TR("Notas", "Notes"), 1, C(255, 255, 255)); tl_x(g);
    if (t->nsel >= nl) t->nsel = nl - 1; if (t->nsel < 0) t->nsel = 0;
    if (t->nsel < t->ntop) t->ntop = t->nsel; if (t->nsel >= t->ntop + TL_ROWS) t->ntop = t->nsel - TL_ROWS + 1;
    if (!nl) draw_wrapped(g, TR("Nenhuma nota. Toque em NOVA para escrever com o teclado.", "No notes. Tap NEW to write one with the keyboard."), 14, 50, 1, 28, 4, 0, C(120, 100, 90), 0);
    for (int r = 0; r < TL_ROWS; r++) {
        int i = t->ntop + r; if (i >= nl) break; int len, s = tl_nline(t, i, &len); char b[66]; if (len > 62) len = 62; for (int k = 0; k < len; k++) b[k] = t->nbuf[s + k]; b[len] = 0;
        int y = 36 + r * 32; rect(g, 4, y, 248, 30, i == t->nsel ? C(255, 226, 150) : C(255, 255, 255)); rect(g, 4, y + 29, 248, 1, C(220, 210, 190));
        draw_wrapped(g, b, 8, y + 3, 1, 30, 2, 0, C(50, 40, 40), 0);
    }
    tl_btn(g, 6, 300, 50, 46, "^", 2, C(200, 190, 170), C(60, 40, 30)); tl_btn(g, 62, 300, 50, 46, "v", 2, C(200, 190, 170), C(60, 40, 30));
    tl_btn(g, 118, 300, 70, 46, TR("NOVA", "NEW"), 2, C(80, 170, 120), C(255, 255, 255)); tl_btn(g, 194, 300, 56, 46, "DEL", 2, C(210, 80, 80), C(255, 255, 255));
    { char c[12]; int n = 0, v = nl; char d[8]; int k = 0; do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v); while (k) c[n++] = d[--k]; c[n] = 0; ptext(g, 8, 360, c, 1, C(120, 100, 90)); ptext(g, 8 + n * 8 + 6, 360, TR("notas", "notes"), 1, C(120, 100, 90)); }
}
static void tl_notes_tap(TL *t) {
    int nl = tl_nlines(t);
    if (tl_in(t, 6, 300, 50, 46)) { if (t->nsel > 0) t->nsel--; }
    else if (tl_in(t, 62, 300, 50, 46)) { if (t->nsel < nl - 1) t->nsel++; }
    else if (tl_in(t, 118, 300, 70, 46)) { if (!t->kbd) t->kbd = kbd_open_cb(t->root, TR("Nova nota", "New note"), TR("Escreva a nota e toque OK", "Type the note and tap OK"), (void *)tl_note_done, t); return; }
    else if (tl_in(t, 194, 300, 56, 46)) { if (nl > 0) { int len, s = tl_nline(t, t->nsel, &len); int e = s + len + 1; for (int i = e; i < t->nlen; i++) t->nbuf[i - (e - s)] = t->nbuf[i]; t->nlen -= e - s; tl_nsave(t); } }
    else if (t->y >= 36 && t->y < 36 + TL_ROWS * 32) { int i = t->ntop + (t->y - 36) / 32; if (i < nl) t->nsel = i; }
    t->dirty = 1;
}

/* ---------------- counter ---------------- */
static void tl_count_draw(TL *t) {
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, C(24, 20, 40)); tl_x(g); ptext(g, 8, 8, TR("Contador", "Counter"), 1, C(180, 170, 210));
    int v = t->cnt < 0 ? -t->cnt : t->cnt, dg = mg_numw(v) + (t->cnt < 0 ? 1 : 0), sc = dg <= 4 ? 8 : dg <= 5 ? 6 : 4, w = dg * 8 * sc;
    if (t->cnt < 0) put(g, 128 - w / 2, 70, '-', sc, C(255, 150, 150)); mg_num(g, 128 - w / 2 + (t->cnt < 0 ? 8 * sc : 0), 70, v, sc, t->cnt < 0 ? C(255, 150, 150) : C(255, 255, 255));
    tl_btn(g, 12, 190, 232, 84, "+1", 4, C(110, 200, 130), C(10, 50, 20)); tl_btn(g, 12, 282, 232, 60, "-1", 3, C(220, 110, 110), C(60, 10, 10)); tl_btn(g, 12, 350, 232, 40, TR("ZERAR", "RESET"), 2, C(80, 76, 110), C(255, 255, 255));
}
static void tl_count_tap(TL *t) {
    if (tl_in(t, 12, 190, 232, 84)) { if (t->cnt < 99999) t->cnt++; MOTOR_ONCE(1, 0); } else if (tl_in(t, 12, 282, 232, 60)) { if (t->cnt > -9999) t->cnt--; } else if (tl_in(t, 12, 350, 232, 40)) t->cnt = 0;
    t->dirty = 1;
}

/* ---------------- dice + coin ---------------- */
static u32 tl_rand(TL *t) { t->rng = t->rng * 1664525u + 1013904223u; return t->rng >> 8; }
static void tl_dice_draw(TL *t) {
    static const char *const nm[4] = { "D6", "2xD6", "D20", "" }; u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, C(18, 40, 30)); tl_x(g); ptext(g, 8, 8, TR("Dados", "Dice"), 1, C(170, 220, 190));
    for (int i = 0; i < 4; i++) tl_btn(g, 8 + i * 62, 36, 58, 36, i < 3 ? nm[i] : TR("MOEDA", "COIN"), 1, i == t->dmode ? C(240, 200, 60) : C(44, 80, 66), i == t->dmode ? C(60, 40, 0) : C(255, 255, 255));
    if (t->dres) {
        if (t->dmode == 3) ctext(g, 128, 130, t->dres == 1 ? TR("CARA", "HEADS") : TR("COROA", "TAILS"), 4, C(255, 230, 120));
        else { int dg = mg_numw(t->dres), w = dg * 8 * 8; mg_num(g, 128 - w / 2, 110, t->dres, 8, C(255, 255, 255)); if (t->dmode == 1) { char s[8] = { (char)('0' + t->dres2 / 10), (char)('0' + t->dres2 % 10), 0 }; ctext(g, 128, 220, s, 2, C(170, 220, 190)); } }
    }
    tl_btn(g, 12, 270, 232, 100, t->dres ? TR("ROLAR DE NOVO", "ROLL AGAIN") : TR("ROLAR", "ROLL"), 2, C(80, 190, 130), C(10, 50, 30));
}
static void tl_dice_tap(TL *t) {
    for (int i = 0; i < 4; i++) if (tl_in(t, 8 + i * 62, 36, 58, 36)) { t->dmode = i; t->dres = 0; t->dirty = 1; return; }
    if (tl_in(t, 12, 270, 232, 100)) {
        t->rng ^= TICK_GET() * 2654435761u;
        switch (t->dmode) { case 0: t->dres = 1 + (int)(tl_rand(t) % 6u); break; case 1: { int a = 1 + (int)(tl_rand(t) % 6u), b = 1 + (int)(tl_rand(t) % 6u); t->dres = a + b; t->dres2 = a * 10 + b; } break;
        case 2: t->dres = 1 + (int)(tl_rand(t) % 20u); break; default: t->dres = 1 + (int)(tl_rand(t) % 2u); break; }
        MOTOR_ONCE(1, 0); t->dirty = 1;
    }
}

/* ---------------- shell ---------------- */
static void tl_draw(TL *t) {
    switch (t->kind) { case TK_GRID: tl_grid_draw(t); break; case TK_CALC: tl_calc_draw(t); break; case TK_CHRONO: tl_chrono_draw(t); break; case TK_LIGHT: tl_light_draw(t); break;
    case TK_NOTES: tl_notes_draw(t); break; case TK_COUNT: tl_count_draw(t); break; default: tl_dice_draw(t); break; }
}
static void tl_go(TL *t, int kind) { t->kind = kind; if (kind == TK_CALC) tl_calc_reset(t); else if (kind == TK_NOTES) { tl_nload(t); t->nsel = tl_nlines(t) - 1; t->ntop = 0; } t->dirty = 1; }
static void tl_close(TL *t) { TIMER_DEL(t->timer); ADD_FLAG(t->img, 1); FREE(t->cvblk); FREE(t); }
static void tl_tick(void *timer) {
    TL *t = *(TL **)((u8 *)timer + 0xC); if (t->kbd) return;
    if (t->kind == TK_CHRONO || t->trun || t->tring) tl_chrono_tick(t);
    if (t->dirty) { t->dirty = 0; tl_draw(t); INVALIDATE(t->img); }
}
static void tl_event(void *e) {
    int code = EV_CODE(e), x, y; TL *t = (TL *)EV_USER(e);
    if (code == 1 || code == 2) { if (touch_read(&x, &y)) { if (!t->pressed && t->kind == TK_CALC) t->dirty = 1; t->pressed = 1; t->x = x; t->y = y; } return; }
    if (code == 8 || code == 3) { t->pressed = 0; if (t->kind == TK_CALC) t->dirty = 1; return; }
    if (code != 7 || t->kbd) return;
    if (t->x >= 216 && t->y < 28) { if (t->kind == TK_GRID) { int fm = t->from_menu, root = 0; void *r = t->root; (void)root; tl_close(t); if (fm) menu_open(r); } else { t->tab = t->tab; tl_go(t, TK_GRID); } return; }
    switch (t->kind) {
    case TK_GRID: for (int i = 0; i < 6; i++) { int tx, ty, tw, th; tl_tile_rect(i, &tx, &ty, &tw, &th); if (tl_in(t, tx, ty, tw, th)) { tl_go(t, i + 1); break; } } break;
    case TK_CALC: tl_calc_tap(t); break; case TK_CHRONO: tl_chrono_tap(t); break; case TK_LIGHT: t->lcol = (t->lcol + 1) % 5; t->dirty = 1; break;
    case TK_NOTES: tl_notes_tap(t); break; case TK_COUNT: tl_count_tap(t); break; default: tl_dice_tap(t); break;
    }
}
static int tl_open(void *root, int from_menu) {
    TL *t = (TL *)MALLOC(sizeof(TL)); if (!t) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!blk) { FREE(t); return 0; }
    for (unsigned i = 0; i < sizeof *t; i++) ((u8 *)t)[i] = 0;
    t->cvblk = blk; t->px = (u16 *)((u8 *)blk + 8); t->root = root; t->from_menu = from_menu; t->kind = TK_GRID; t->rng = TICK_GET() * 2654435761u + 99u; t->tset = 5 * 60000; t->tleft = t->tset; tl_calc_reset(t);
    tl_grid_draw(t);
    t->img = make_canvas(root, t->dsc, t->px, (void *)tl_event, t);
    t->timer = TIMER_CREATE((void *)tl_tick, 33, t);
    return 1;
}
