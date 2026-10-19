/* Dice roller: tap to roll one to four dice (buttons at the top choose how many). Shows the pips and the total. */
#include "f3app.h"

static const F3Api *A;
typedef struct { F3App base; int n, v[4]; u32 seed, roll_t; int rolling; } St;

static u32 rnd(St *s) { s->seed = s->seed * 1664525u + 1013904223u; return s->seed >> 8; }
static void die(int cx, int cy, int v) {
    A->rrect(A->fb, cx - 40, cy - 40, 80, 80, 14, RGB(250, 250, 245));
    static const signed char P[7][9][2] = { {{0}}, { { 0, 0 } }, { { -18, -18 }, { 18, 18 } }, { { -18, -18 }, { 0, 0 }, { 18, 18 } }, { { -18, -18 }, { 18, -18 }, { -18, 18 }, { 18, 18 } },
        { { -18, -18 }, { 18, -18 }, { 0, 0 }, { -18, 18 }, { 18, 18 } }, { { -18, -20 }, { 18, -20 }, { -18, 0 }, { 18, 0 }, { -18, 20 }, { 18, 20 } } };
    static const signed char C[7] = { 0, 1, 2, 3, 4, 5, 6 };
    for (int i = 0; i < C[v]; i++) A->disc(A->fb, cx + P[v][i][0], cy + P[v][i][1], 7, RGB(30, 30, 40));
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed; s->seed ^= now;
    if (tap) { if (y < 70) { int k = x / 64 + 1; if (k >= 1 && k <= 4) s->n = k; } else { s->rolling = 1; s->roll_t = now; A->vibrate(1); } }
    if (s->rolling) { for (int i = 0; i < 4; i++) s->v[i] = 1 + (int)(rnd(s) % 6u); if (now - s->roll_t > 600) { s->rolling = 0; A->vibrate(4); } }
    A->rect(A->fb, 0, 0, F3_W, F3_H, RGB(14, 40, 30));
    for (int i = 0; i < 4; i++) { A->rrect(A->fb, 6 + i * 64, 34, 58, 30, 8, i + 1 == s->n ? RGB(240, 200, 60) : RGB(40, 84, 66)); A->num(A->fb, 28 + i * 64, 40, i + 1, 2, i + 1 == s->n ? RGB(60, 40, 0) : RGB(255, 255, 255)); }
    int total = 0;
    for (int i = 0; i < s->n; i++) { int cx = s->n == 1 ? 128 : (i % 2 ? 190 : 66), cy = s->n <= 2 ? (s->n == 1 ? 190 : 150 + i * 100) : 130 + (i / 2) * 110; if (s->n == 2) { cx = 128; } die(cx, cy, s->v[i]); total += s->v[i]; }
    A->ctext(A->fb, 128, 340, A->lang_pt ? "toque para rolar" : "tap to roll", 1, RGB(200, 230, 215));
    A->text(A->fb, 90, 360, "=", 3, RGB(255, 220, 120)); A->num(A->fb, 120, 360, total, 3, RGB(255, 255, 255));
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    s->base.tick = tick; s->base.close = close_; s->n = 2; s->seed = api->ticks() * 2654435761u + 5u; s->rolling = 0; for (int i = 0; i < 4; i++) s->v[i] = 1 + i; return &s->base;
}
