#include "kbd.h"
#include "kbd_font.h"

#define NKEYS 48
typedef struct { int16_t x, y, w, h; uint8_t act; uint16_t cp; } Key;

/* ---------- layout (shared by draw and touch) ---------- */
static void add(Key *ks, int *n, int x, int y, int w, int h, int act, int cp) {
    if (*n < NKEYS) { Key *k = &ks[(*n)++]; k->x = (int16_t)x; k->y = (int16_t)y; k->w = (int16_t)w; k->h = (int16_t)h; k->act = (uint8_t)act; k->cp = (uint16_t)cp; }
}
static void row(Key *ks, int *n, int y, int h, const uint16_t *cps, int cnt, int lead2, int trail2, int lead_act, int trail_act) {
    /* units are half-keys (so 3 = 1.5 keys); total 20 half-units across the row */
    int total = 2 * cnt + lead2 + trail2, i, x = 3 * 16;                        /* x in 1/16 px */
    int uw16 = ((KBD_W - 6) * 16) / total;
    if (lead2) { add(ks, n, x / 16, y, (lead2 * uw16) / 16 - 2, h, lead_act, 0); x += lead2 * uw16; }
    for (i = 0; i < cnt; i++) { add(ks, n, x / 16, y, (2 * uw16) / 16 - 2, h, KA_CHAR, cps[i]); x += 2 * uw16; }
    if (trail2) add(ks, n, x / 16, y, (trail2 * uw16) / 16 - 2, h, trail_act, 0);
}
static int build(const Kbd *k, Key *ks) {
    static const uint16_t l1[] = {'q','w','e','r','t','y','u','i','o','p'}, l2[] = {'a','s','d','f','g','h','j','k','l',231}, l3[] = {'z','x','c','v','b','n','m'};
    static const uint16_t L1[] = {'Q','W','E','R','T','Y','U','I','O','P'}, L2[] = {'A','S','D','F','G','H','J','K','L',199}, L3[] = {'Z','X','C','V','B','N','M'};
    static const uint16_t s1[] = {'1','2','3','4','5','6','7','8','9','0'}, s2[] = {'@','#','$','%','&','-','+','(',')','/'}, s3[] = {'?','!','"','\'',':',';',',','.','='};
    int n = 0, y0 = 150, h = 60, g = 4;
    add(ks, &n, KBD_W - 44, 2, 42, 26, KA_CANCEL, 0);
    if (k->sym) {
        row(ks, &n, y0, h, s1, 10, 0, 0, 0, 0); row(ks, &n, y0 + (h + g), h, s2, 10, 0, 0, 0, 0);
        row(ks, &n, y0 + 2 * (h + g), h, s3, 9, 0, 0, 0, 0);
    } else {
        row(ks, &n, y0, h, k->shift ? L1 : l1, 10, 0, 0, 0, 0); row(ks, &n, y0 + (h + g), h, k->shift ? L2 : l2, 10, 0, 0, 0, 0);
        row(ks, &n, y0 + 2 * (h + g), h, k->shift ? L3 : l3, 7, 3, 3, KA_SHIFT, KA_BACK);
    }
    {   /* bottom row: [123/ABC] [accent] [ space ......] [bksp if sym] [send] ; widths in px */
        int y = y0 + 3 * (h + g), x = 3;
        add(ks, &n, x, y, 44, h, KA_SYM, 0); x += 46;
        add(ks, &n, x, y, 44, h, KA_ACCENT, 0); x += 46;
        add(ks, &n, x, y, 70, h, KA_SPACE, ' '); x += 72;
        add(ks, &n, x, y, 36, h, KA_BACK, 0); x += 38;
        add(ks, &n, x, y, KBD_W - 3 - x, h, KA_SEND, 0);
    }
    return n;
}

/* ---------- text helpers ---------- */
static int utf8_put(char *d, uint32_t cp) {
    if (cp < 0x80) { d[0] = (char)cp; return 1; }
    if (cp < 0x800) { d[0] = (char)(0xC0 | (cp >> 6)); d[1] = (char)(0x80 | (cp & 63)); return 2; }
    d[0] = (char)(0xE0 | (cp >> 12)); d[1] = (char)(0x80 | ((cp >> 6) & 63)); d[2] = (char)(0x80 | (cp & 63)); return 3;
}
static void append(Kbd *k, uint32_t cp) {
    char t[4]; int n = utf8_put(t, cp);
    if (k->len + n > KBD_MAX) return;
    for (int i = 0; i < n; i++) k->buf[k->len++] = t[i]; k->buf[k->len] = 0;
}
static void backspace(Kbd *k) {
    if (k->len <= 0) return;
    do k->len--; while (k->len > 0 && ((uint8_t)k->buf[k->len] & 0xC0) == 0x80);
    k->buf[k->len] = 0;
}
static uint32_t compose(int accent, uint32_t c) {
    static const struct { uint16_t base, r[4]; } t[] = {                 /* acute, circumflex, tilde, grave */
        {'a', {225, 226, 227, 224}}, {'e', {233, 234, 'e', 232}}, {'i', {237, 238, 'i', 236}}, {'o', {243, 244, 245, 242}}, {'u', {250, 251, 'u', 249}},
        {'n', {'n', 'n', 241, 'n'}},
        {'A', {193, 194, 195, 192}}, {'E', {201, 202, 'E', 200}}, {'I', {205, 'I', 'I', 204}}, {'O', {211, 212, 213, 210}}, {'U', {218, 219, 'U', 217}},
        {'N', {'N', 'N', 209, 'N'}},
    };
    if (accent < 1 || accent > 4) return c;
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if (t[i].base == c) return t[i].r[accent - 1];
    return c;
}

void kbd_init(Kbd *k, const char *init) {
    k->len = 0; k->buf[0] = 0; k->shift = 1; k->sym = 0; k->accent = 0; k->state = KBD_EDIT; k->has_msg = 0; k->title[0] = 0; k->body[0] = 0;
    if (init) while (*init && k->len < KBD_MAX) { k->buf[k->len++] = *init++; k->buf[k->len] = 0; }
}
static void copy_str(char *d, const char *s, int max) {          /* bounded copy that never splits a UTF-8 sequence */
    int i = 0; if (!s) { d[0] = 0; return; }
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    while (i > 0 && s[i] && ((uint8_t)s[i] & 0xC0) == 0x80) i--;   /* cut landed inside a sequence: back up */
    d[i] = 0;
}
void kbd_set_msg(Kbd *k, const char *title, const char *body) { copy_str(k->title, title, (int)sizeof k->title); copy_str(k->body, body, (int)sizeof k->body); k->has_msg = 1; }
void kbd_touch(Kbd *k, int x, int y) {
    Key ks[NKEYS]; int n = build(k, ks);
    for (int i = 0; i < n; i++) {
        const Key *e = &ks[i];
        if (x < e->x || x >= e->x + e->w + 2 || y < e->y || y >= e->y + e->h + 2) continue;
        switch (e->act) {
        case KA_CHAR: { uint32_t c = compose(k->accent, e->cp); append(k, c); k->accent = 0; if (k->shift && !k->sym && k->len > 1) k->shift = 0; break; }
        case KA_SPACE: append(k, ' '); k->accent = 0; k->shift = (k->len >= 2 && (k->buf[k->len - 2] == '.' || k->buf[k->len - 2] == '?' || k->buf[k->len - 2] == '!')); break;
        case KA_SHIFT: k->shift = !k->shift; break;
        case KA_BACK: backspace(k); k->accent = 0; break;
        case KA_SYM: k->sym = !k->sym; break;
        case KA_ACCENT: k->accent = (uint8_t)((k->accent + 1) % 5); break;
        case KA_SEND: if (k->len > 0) k->state = KBD_SEND; break;
        case KA_CANCEL: k->state = KBD_CANCEL; break;
        }
        return;
    }
}
int kbd_find_key(const Kbd *k, int cp, int *cx, int *cy) {
    Key ks[NKEYS]; int n = build(k, ks);
    for (int i = 0; i < n; i++) if (ks[i].act == KA_CHAR && ks[i].cp == cp) { *cx = ks[i].x + ks[i].w / 2; *cy = ks[i].y + ks[i].h / 2; return 1; }
    return 0;
}
int kbd_find_action(const Kbd *k, int act, int *cx, int *cy) {
    Key ks[NKEYS]; int n = build(k, ks);
    for (int i = 0; i < n; i++) if (ks[i].act == act) { *cx = ks[i].x + ks[i].w / 2; *cy = ks[i].y + ks[i].h / 2; return 1; }
    return 0;
}

/* ---------- drawing ---------- */
#define C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static void rect(uint16_t *g_fb, int x, int y, int w, int h, uint16_t c) {
    for (int j = y; j < y + h; j++) { if (j < 0 || j >= KBD_H) continue; for (int i = x; i < x + w; i++) if (i >= 0 && i < KBD_W) g_fb[j * KBD_W + i] = c; }
}
static const uint8_t *glyph(uint32_t cp) {
    if (cp >= 32 && cp <= 126) return kbd_font[cp - 32];
    for (int i = 0; i < (int)(sizeof kbd_font_extra_cp / 2); i++) if (kbd_font_extra_cp[i] == cp) return kbd_font[KBD_FONT_N_ASCII + i];
    return kbd_font['?' - 32];
}
static void put(uint16_t *g_fb, int x, int y, uint32_t cp, int s, uint16_t c) {
    const uint8_t *g = glyph(cp);
    for (int j = 0; j < 12; j++) for (int i = 0; i < 8; i++) if (g[j] & (0x80 >> i)) rect(g_fb, x + i * s, y + j * s, s, s, c);
}
static void ptext(uint16_t *g_fb, int x, int y, const char *s, int sc, uint16_t c) { for (; *s; s++) { put(g_fb, x, y, (uint8_t)*s, sc, c); x += 8 * sc; } }
static uint32_t utf8_get(const char *s, int *adv) {
    uint8_t c = (uint8_t)s[0];
    if (c < 0x80) { *adv = 1; return c; }
    if ((c & 0xE0) == 0xC0) { *adv = 2; return ((uint32_t)(c & 31) << 6) | ((uint8_t)s[1] & 63); }
    *adv = 3; return ((uint32_t)(c & 15) << 12) | (((uint32_t)(uint8_t)s[1] & 63) << 6) | ((uint8_t)s[2] & 63);
}
/* word-free character wrap. Draws up to maxlines lines of cols columns; from_end: show the LAST maxlines lines (typing view).
 * cursor != 0: draw an underline cursor after the last character. '(newline)' forces a new line. */
static void draw_wrapped(uint16_t *g_fb, const char *s, int x, int y, int sc, int cols, int maxlines, int from_end, uint16_t color, uint16_t cursor) {
    int pass, line, col, adv, total = 0; const char *p;
    for (pass = 0; pass < 2; pass++) {
        int first = (pass == 1 && from_end && total > maxlines) ? total - maxlines : 0;
        line = 0; col = 0;
        for (p = s; ; p += adv) {
            uint32_t cp = 0; int end = (*p == 0);
            if (!end) cp = utf8_get(p, &adv); else adv = 0;
            if (!end && cp == 10) { line++; col = 0; continue; }
            if (col == cols) { line++; col = 0; }
            if (pass == 1 && line >= first && line < first + maxlines) {
                int px = x + col * 8 * sc, py = y + (line - first) * (12 * sc + 1);
                if (!end) put(g_fb, px, py, cp, sc, color); else if (cursor) rect(g_fb, px, py + 12 * sc - 2, 7 * sc, 2, cursor);
            }
            if (end) break;
            col++;
        }
        total = line + 1;
    }
}
void kbd_draw(const Kbd *k, uint16_t *g_fb) {
    Key ks[NKEYS]; int n = build(k, ks);
    rect(g_fb, 0, 0, KBD_W, KBD_H, C(16, 16, 20));
    /* message panel (what we are replying to) */
    rect(g_fb, 0, 0, KBD_W, 76, C(22, 26, 42));
    if (k->has_msg) {
        draw_wrapped(g_fb, k->title, 4, 3, 1, 26, 1, 0, C(255, 210, 90), 0);
        draw_wrapped(g_fb, k->body[0] ? k->body : "(sem texto)", 4, 18, 1, 31, 5, 0, C(235, 235, 240), 0);
    } else draw_wrapped(g_fb, "Digite a resposta", 4, 8, 1, 26, 1, 0, C(150, 160, 190), 0);
    /* reply box: last 2 lines of what has been typed, with cursor */
    rect(g_fb, 2, 80, KBD_W - 4, 66, C(30, 32, 40));
    draw_wrapped(g_fb, k->buf, 6, 85, 2, 15, 2, 1, C(255, 255, 255), C(120, 200, 255));
    /* header: accent indicator */
    { if (k->accent) { static const char marks[] = {0, 39, '^', '~', 96}; ptext(g_fb, 6, 10, "acento:", 1, C(160, 160, 170)); put(g_fb, 70, 6, (uint32_t)marks[k->accent], 2, C(255, 210, 90)); } }
    for (int i = 0; i < n; i++) {
        const Key *e = &ks[i]; uint16_t bg = C(52, 54, 62), fg = C(240, 240, 240);
        if (e->act == KA_SEND) bg = C(30, 140, 70); else if (e->act == KA_CANCEL) bg = C(150, 40, 40);
        else if (e->act != KA_CHAR && e->act != KA_SPACE) bg = C(35, 80, 130);
        if ((e->act == KA_SHIFT && k->shift) || (e->act == KA_ACCENT && k->accent)) bg = C(200, 140, 20);
        rect(g_fb, e->x, e->y, e->w, e->h, bg);
        if (e->act == KA_CHAR) put(g_fb, e->x + e->w / 2 - 8, e->y + e->h / 2 - 12, e->cp, 2, fg);
        else {
            const char *l = e->act == KA_SHIFT ? "^" : e->act == KA_BACK ? "<" : e->act == KA_SYM ? (k->sym ? "ABC" : "123") : e->act == KA_ACCENT ? "'^~" :
                            e->act == KA_SEND ? "OK" : e->act == KA_CANCEL ? "X" : "";
            int len = 0; while (l[len]) len++; int sc = (e->act == KA_ACCENT || e->act == KA_SYM) ? 1 : 2;
            ptext(g_fb, e->x + e->w / 2 - len * 4 * sc, e->y + e->h / 2 - 6 * sc, l, sc, fg);
            if (e->act == KA_SPACE) rect(g_fb, e->x + 10, e->y + e->h / 2 - 1, e->w - 20, 3, C(190, 190, 190));
        }
    }
}
