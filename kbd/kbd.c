#include "kbd.h"
#include "kbd_font.h"

/* ================= palette + drawing primitives (shared with the launcher via include) ================= */
#define C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define P_BG     C(246, 242, 250)          /* soft lavender white */
#define P_CARD   C(255, 255, 255)
#define P_SHADOW C(218, 210, 230)
#define P_TEXT   C(58, 50, 74)
#define P_MUTED  C(140, 130, 160)
#define P_ACCENT C(124, 88, 190)

static uint16_t mix565(uint16_t a, uint16_t b, int t) {                        /* t = weight of b, 0..16 */
    int ra = a >> 11, ga = (a >> 5) & 63, ba = a & 31, rb = b >> 11, gb = (b >> 5) & 63, bb = b & 31;
    return (uint16_t)((((ra * (16 - t) + rb * t) >> 4) << 11) | (((ga * (16 - t) + gb * t) >> 4) << 5) | ((ba * (16 - t) + bb * t) >> 4));
}
static void rect(uint16_t *g_fb, int x, int y, int w, int h, uint16_t c) {
    for (int j = y; j < y + h; j++) { if (j < 0 || j >= KBD_H) continue; for (int i = x; i < x + w; i++) if (i >= 0 && i < KBD_W) g_fb[j * KBD_W + i] = c; }
}
/* Rounded rectangle with anti-aliased corners (integer maths only: coverage from the squared distance, no sqrt). */
static void rrect(uint16_t *g, int x, int y, int w, int h, int r, uint16_t col) {
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 1) { rect(g, x, y, w, h, col); return; }
    int hi = (2 * r + 1) * (2 * r + 1), lo = (2 * r - 1) * (2 * r - 1);
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= KBD_H) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= KBD_W) continue;
            int ax = 0, ay = 0;
            if (i < x + r) ax = 2 * (x + r) - 2 * i - 1; else if (i >= x + w - r) ax = 2 * i + 1 - 2 * (x + w - r);
            if (j < y + r) ay = 2 * (y + r) - 2 * j - 1; else if (j >= y + h - r) ay = 2 * j + 1 - 2 * (y + h - r);
            if (ax && ay) {
                int D = ax * ax + ay * ay;
                if (D >= hi) continue;
                if (D > lo) { g[j * KBD_W + i] = mix565(g[j * KBD_W + i], col, 16 * (hi - D) / (hi - lo)); continue; }
            }
            g[j * KBD_W + i] = col;
        }
    }
}
static void disc(uint16_t *g, int cx, int cy, int r, uint16_t col) { rrect(g, cx - r, cy - r, 2 * r, 2 * r, r, col); }
/* a card: soft drop shadow + rounded body */
static void card(uint16_t *g, int x, int y, int w, int h, int r, uint16_t col) { rrect(g, x, y + 3, w, h, r, P_SHADOW); rrect(g, x, y, w, h, r, col); }

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
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void ctext(uint16_t *g_fb, int cx, int y, const char *s, int sc, uint16_t c) { ptext(g_fb, cx - slen(s) * 4 * sc, y, s, sc, c); }
static uint32_t utf8_get(const char *s, int *adv) {
    uint8_t c = (uint8_t)s[0];
    if (c < 0x80) { *adv = 1; return c; }
    if ((c & 0xE0) == 0xC0) { *adv = 2; return ((uint32_t)(c & 31) << 6) | ((uint8_t)s[1] & 63); }
    *adv = 3; return ((uint32_t)(c & 15) << 12) | (((uint32_t)(uint8_t)s[1] & 63) << 6) | ((uint8_t)s[2] & 63);
}
/* word-free character wrap. Draws up to maxlines lines of cols columns; from_end: show the LAST maxlines lines (typing view).
 * cursor != 0: draw an underline cursor after the last character. '\n' forces a new line. */
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

/* ================= T9 layout (shared by draw and touch) ================= */
#define NKEYS 16
typedef struct { int16_t x, y, w, h; uint8_t act, cp; } Key;
static void add(Key *ks, int *n, int x, int y, int w, int h, int act, int cp) {
    if (*n < NKEYS) { Key *k = &ks[(*n)++]; k->x = (int16_t)x; k->y = (int16_t)y; k->w = (int16_t)w; k->h = (int16_t)h; k->act = (uint8_t)act; k->cp = (uint8_t)cp; }
}
static int build(Key *ks) {
    int n = 0, d;
    add(ks, &n, KBD_W - 46, 4, 42, 28, KA_CANCEL, 0);
    add(ks, &n, 184, 80, 66, 66, KA_SEND, 0);                                     /* OK sits next to the reply box */
    for (d = 1; d <= 9; d++) add(ks, &n, 6 + ((d - 1) % 3) * 83, 154 + ((d - 1) / 3) * 62, 78, 56, KA_T9, d);
    add(ks, &n, 6, 340, 52, 52, KA_BACK, 0);
    add(ks, &n, 62, 340, 88, 52, KA_SPACE, 0);
    add(ks, &n, 154, 340, 52, 52, KA_MODE, 0);
    add(ks, &n, 210, 340, 40, 52, KA_ACCENT, 0);
    return n;
}

/* ================= multi-tap tables ================= */
static const uint16_t t9_1[] = {'.', ',', '?', '!', '\'', '-', ':', ';', '(', ')', '@', '1', '0'};
static const uint16_t t9_2[] = {'a', 'b', 'c', 231, '2'}, t9_3[] = {'d', 'e', 'f', '3'}, t9_4[] = {'g', 'h', 'i', '4'};
static const uint16_t t9_5[] = {'j', 'k', 'l', '5'}, t9_6[] = {'m', 'n', 'o', '6'}, t9_7[] = {'p', 'q', 'r', 's', '7'};
static const uint16_t t9_8[] = {'t', 'u', 'v', '8'}, t9_9[] = {'w', 'x', 'y', 'z', '9'};
static const struct { const uint16_t *c; uint8_t n; const char *lbl; } t9[10] = {
    {0, 0, ""}, {t9_1, 13, ".,?"}, {t9_2, 5, "abc"}, {t9_3, 4, "def"}, {t9_4, 4, "ghi"}, {t9_5, 4, "jkl"}, {t9_6, 4, "mno"}, {t9_7, 5, "pqrs"}, {t9_8, 4, "tuv"}, {t9_9, 5, "wxyz"} };
int kbd_t9_len(int d) { return (d >= 1 && d <= 9) ? t9[d].n : 0; }
int kbd_t9_index(int d, int cp) { if (d < 1 || d > 9) return -1; for (int i = 0; i < t9[d].n; i++) if (t9[d].c[i] == cp) return i; return -1; }

/* ================= text helpers ================= */
static int utf8_put(char *d, uint32_t cp) {
    if (cp < 0x80) { d[0] = (char)cp; return 1; }
    if (cp < 0x800) { d[0] = (char)(0xC0 | (cp >> 6)); d[1] = (char)(0x80 | (cp & 63)); return 2; }
    d[0] = (char)(0xE0 | (cp >> 12)); d[1] = (char)(0x80 | ((cp >> 6) & 63)); d[2] = (char)(0x80 | (cp & 63)); return 3;
}
static void append(Kbd *k, uint32_t cp) {
    char t[4]; int n = utf8_put(t, cp);
    if (k->len + n > KBD_MAX) return;
    for (int i = 0; i < n; i++) k->buf[k->len++] = t[i];
    k->buf[k->len] = 0;
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
static uint32_t upper(uint32_t c) { if (c >= 'a' && c <= 'z') return c - 32; if (c == 231) return 199; return c; }
static int upper_ctx(const Kbd *k) {                                    /* should the next letter be a capital? */
    int n = k->len;
    if (k->caps == 2) return 1;
    if (k->caps == 1) return 0;
    if (n == 0) return 1;
    return n >= 2 && k->buf[n - 1] == ' ' && (k->buf[n - 2] == '.' || k->buf[n - 2] == '?' || k->buf[n - 2] == '!');
}

void kbd_init(Kbd *k, const char *init) {
    k->len = 0; k->buf[0] = 0; k->caps = 0; k->accent = 0; k->state = KBD_EDIT; k->has_msg = 0; k->en = 0; k->title[0] = 0; k->body[0] = 0;
    k->last_key = 0xFF; k->last_idx = 0; k->cur_upper = 0; k->acc_use = 0; k->last_time = 0;
    if (init) while (*init && k->len < KBD_MAX) { k->buf[k->len++] = *init++; k->buf[k->len] = 0; }
}
static void copy_str(char *d, const char *s, int max) {          /* bounded copy that never splits a UTF-8 sequence */
    int i = 0; if (!s) { d[0] = 0; return; }
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    while (i > 0 && s[i] && ((uint8_t)s[i] & 0xC0) == 0x80) i--;   /* cut landed inside a sequence: back up */
    d[i] = 0;
}
void kbd_set_msg(Kbd *k, const char *title, const char *body) { copy_str(k->title, title, (int)sizeof k->title); copy_str(k->body, body, (int)sizeof k->body); k->has_msg = 1; }

void kbd_touch(Kbd *k, int x, int y, uint32_t now) {
    Key ks[NKEYS]; int n = build(ks);
    for (int i = 0; i < n; i++) {
        const Key *e = &ks[i];
        if (x < e->x || x >= e->x + e->w + 2 || y < e->y || y >= e->y + e->h + 2) continue;
        if (e->act == KA_T9) {
            int d = e->cp; int cyc = (k->last_key == d && k->len > 0 && (uint32_t)(now - k->last_time) < 900u);
            if (cyc) { k->last_idx = (uint8_t)((k->last_idx + 1) % t9[d].n); backspace(k); }
            else { k->last_idx = 0; k->cur_upper = (uint8_t)upper_ctx(k); k->acc_use = k->accent; k->accent = 0; }
            { uint32_t c = t9[d].c[k->last_idx]; if (k->cur_upper) c = upper(c); append(k, compose(k->acc_use, c)); }
            k->last_key = (uint8_t)d; k->last_time = now;
            return;
        }
        k->last_key = 0xFF;                                             /* any other key ends the multi-tap run */
        switch (e->act) {
        case KA_SPACE: append(k, ' '); k->accent = 0; break;
        case KA_BACK: backspace(k); k->accent = 0; break;
        case KA_MODE: k->caps = (uint8_t)((k->caps + 1) % 3); break;
        case KA_ACCENT: k->accent = (uint8_t)((k->accent + 1) % 5); break;
        case KA_SEND: if (k->len > 0) k->state = KBD_SEND; break;
        case KA_CANCEL: k->state = KBD_CANCEL; break;
        }
        return;
    }
}
int kbd_find_t9(const Kbd *k, int d, int *cx, int *cy) {
    Key ks[NKEYS]; int n = build(ks); (void)k;
    for (int i = 0; i < n; i++) if (ks[i].act == KA_T9 && ks[i].cp == d) { *cx = ks[i].x + ks[i].w / 2; *cy = ks[i].y + ks[i].h / 2; return 1; }
    return 0;
}
int kbd_find_action(const Kbd *k, int act, int *cx, int *cy) {
    Key ks[NKEYS]; int n = build(ks); (void)k;
    for (int i = 0; i < n; i++) if (ks[i].act == act) { *cx = ks[i].x + ks[i].w / 2; *cy = ks[i].y + ks[i].h / 2; return 1; }
    return 0;
}

/* ================= drawing ================= */
void kbd_draw(const Kbd *k, uint16_t *g_fb) {
    Key ks[NKEYS]; int n = build(ks), i;
    rect(g_fb, 0, 0, KBD_W, KBD_H, P_BG);
    /* message card (what we are replying to) */
    card(g_fb, 6, 4, KBD_W - 12, 70, 18, P_CARD);
    if (k->has_msg) {
        draw_wrapped(g_fb, k->title, 14, 9, 1, 22, 1, 0, P_ACCENT, 0);
        draw_wrapped(g_fb, k->body[0] ? k->body : (k->en ? "(no text)" : "(sem texto)"), 14, 24, 1, 29, 4, 0, P_TEXT, 0);
    } else draw_wrapped(g_fb, (k->en ? "Type your reply" : "Digite a resposta"), 14, 24, 1, 22, 1, 0, P_MUTED, 0);
    /* reply box: last 2 lines of what has been typed, with cursor */
    card(g_fb, 6, 80, 174, 66, 18, P_CARD);
    draw_wrapped(g_fb, k->buf, 14, 86, 2, 10, 2, 1, P_TEXT, P_ACCENT);
    for (i = 0; i < n; i++) {
        const Key *e = &ks[i]; int cx = e->x + e->w / 2, cy = e->y + e->h / 2;
        if (e->act == KA_T9) {
            card(g_fb, e->x, e->y, e->w, e->h, 18, P_CARD);
            { char d[2]; d[0] = (char)('0' + e->cp); d[1] = 0; ctext(g_fb, cx, e->y + 3, d, 2, P_TEXT); ctext(g_fb, cx, e->y + 29, t9[e->cp].lbl, 2, P_ACCENT); }
        } else if (e->act == KA_SEND) {
            card(g_fb, e->x, e->y, e->w, e->h, 22, C(132, 214, 168)); ctext(g_fb, cx, cy - 12, "OK", 3, C(24, 92, 60));
        } else if (e->act == KA_CANCEL) {
            card(g_fb, e->x, e->y, e->w, e->h, 14, C(246, 176, 188)); ctext(g_fb, cx, cy - 9, "X", 2, C(130, 36, 56));
        } else if (e->act == KA_BACK) {
            card(g_fb, e->x, e->y, e->w, e->h, 18, C(250, 200, 208)); ctext(g_fb, cx, cy - 12, "<", 3, C(130, 36, 56));
        } else if (e->act == KA_SPACE) {
            card(g_fb, e->x, e->y, e->w, e->h, 18, C(214, 238, 226)); rrect(g_fb, e->x + 22, cy - 2, e->w - 44, 5, 2, C(80, 140, 110));
        } else if (e->act == KA_MODE) {
            card(g_fb, e->x, e->y, e->w, e->h, 18, C(214, 200, 244)); ctext(g_fb, cx, cy - 12, k->caps == 0 ? "Abc" : k->caps == 1 ? "abc" : "ABC", 2, P_ACCENT);
        } else {                                                           /* accent */
            card(g_fb, e->x, e->y, e->w, e->h, 18, k->accent ? C(250, 196, 150) : C(252, 226, 204));
            { static const char marks[] = {39, 39, '^', '~', 96}; put(g_fb, cx - 8, cy - 12, (uint32_t)marks[k->accent], 2, C(150, 84, 20)); }
        }
    }
}
