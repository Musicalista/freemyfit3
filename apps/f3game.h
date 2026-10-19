/* Shared helpers for the Fit3 game apps (header-only; everything is static so unused helpers cost nothing). Needs f3app.h. */
#ifndef F3GAME_H
#define F3GAME_H
#include "f3app.h"

static const F3Api *A;
#define TR(pt, en) (A->lang_pt ? (pt) : (en))
#define FB A->fb
#define C_BG RGB(14, 16, 28)
#define C_TEXT RGB(236, 240, 250)
#define C_MUTED RGB(140, 150, 175)

static u32 rng_next(u32 *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }
static int iabs(int v) { return v < 0 ? -v : v; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void clear(u16 c) { A->rect(FB, 0, 0, F3_W, F3_H, c); }
static void fill8(void *p, int v, u32 n) { u8 *b = (u8 *)p; while (n--) *b++ = (u8)v; }

/* one finger, classified on release: tap, or swipe in 4 directions (x = last position while pressed) */
typedef struct { int down, sx, sy, lx, ly; } Touch;
enum { T_NONE, T_TAP, T_LEFT, T_RIGHT, T_UP, T_DOWN };
static int touch_update(Touch *t, int pressed, int x, int y) {
    if (pressed) { if (!t->down) { t->down = 1; t->sx = x; t->sy = y; } t->lx = x; t->ly = y; return T_NONE; }
    if (!t->down) return T_NONE;
    t->down = 0; { int dx = t->lx - t->sx, dy = t->ly - t->sy, ax = iabs(dx), ay = iabs(dy);
        if (ax < 16 && ay < 16) return T_TAP;
        if (ax >= ay) return dx < 0 ? T_LEFT : T_RIGHT;
        return dy < 0 ? T_UP : T_DOWN; }
}
static int in_rect(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

/* best score kept in /user/<name> (4 bytes); silently skipped when the file system refuses */
static int hs_load(const char *path) { int v = 0, fd = A->fopen(path, "r"); if (fd < 0) return 0; if (A->fread(fd, &v, 4) != 4) v = 0; A->fclose(fd); return v < 0 ? 0 : v; }
static void hs_save(const char *path, int v) { int fd = A->fopen(path, "w"); if (fd < 0) return; A->fwrite(fd, &v, 4); A->fclose(fd); }

static void title(const char *s, int score, int best) {                         /* top bar; the watch draws its own X at the top right (x >= 216) */
    A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, s, 2, C_TEXT);
    if (score >= 0) { A->num(FB, 124, 2, score, 2, RGB(255, 220, 120)); A->text(FB, 124, 22, TR("rec", "best"), 1, C_MUTED); A->num(FB, 160, 22, best, 1, C_MUTED); }
}
static void over_box(const char *msg, int score) {
    A->rrect(FB, 28, 150, 200, 100, 14, RGB(30, 34, 60)); A->ctext(FB, 128, 160, msg, 2, RGB(255, 120, 110));
    A->text(FB, 60, 196, TR("pontos", "score"), 1, C_MUTED); A->num(FB, 130, 190, score, 2, C_TEXT); A->ctext(FB, 128, 226, TR("toque p/ jogar", "tap to play"), 1, C_TEXT);
}
/* int -> text without libc: writes at most 11 chars + NUL, min `pad` digits (zero padded); returns b */
static char *itoa_pad(char *b, int v, int pad) {
    char t[12]; int n = 0, neg = v < 0; u32 u = neg ? (u32)(-v) : (u32)v; do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u && n < 10); while (n < pad && n < 10) t[n++] = '0';
    int k = 0; if (neg) b[k++] = '-'; while (n) b[k++] = t[--n]; b[k] = 0; return b;
}
/* long-press aware finger: kind 0 none, 1 tap (released quickly), 2 long press (fires once while still down) */
typedef struct { int down, x, y, t0, fired, moved; } Press;
static int press_update(Press *p, int pressed, int x, int y) {
    u32 now = A->ticks();
    if (pressed) {
        if (!p->down) { p->down = 1; p->t0 = (int)now; p->fired = 0; p->moved = 0; p->x = x; p->y = y; }
        if (iabs(x - p->x) > 14 || iabs(y - p->y) > 14) p->moved = 1;
        if (!p->fired && !p->moved && (int)(now - (u32)p->t0) > 450) { p->fired = 1; return 2; }
        return 0;
    }
    if (!p->down) return 0;
    p->down = 0; return (!p->fired && !p->moved) ? 1 : 0;
}
#endif
