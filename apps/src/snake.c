/* Snake: swipe to steer (or tap the left/right half to turn). Eat the dots, do not hit the wall or yourself. */
#include "f3game.h"

#define COLS 16
#define ROWS 22
#define CELL 16
#define OY 44
#define HS "/user/snake.sav"
typedef struct { F3App base; Touch t; u32 rng, last; int len, dir, want, fx, fy, score, best, over, ms, over_t; u8 bx[COLS * ROWS], by[COLS * ROWS]; } St;
static const signed char DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };    /* 0 up, 1 right, 2 down, 3 left */

static void place_food(St *s) {
    for (int tries = 0; tries < 400; tries++) {
        int x = (int)(rng_next(&s->rng) % COLS), y = (int)(rng_next(&s->rng) % ROWS), ok = 1;
        for (int i = 0; i < s->len; i++) if (s->bx[i] == x && s->by[i] == y) { ok = 0; break; }
        if (ok) { s->fx = x; s->fy = y; return; }
    }
    s->fx = -1; s->fy = -1;
}
static void reset(St *s) {
    s->len = 3; s->dir = s->want = 1; for (int i = 0; i < 3; i++) { s->bx[i] = (u8)(6 - i); s->by[i] = ROWS / 2; }
    s->score = 0; s->over = 0; s->ms = 190; place_food(s);
}
static void turn(St *s, int d) { if (((d + 2) & 3) != s->dir) s->want = d; }
static void advance(St *s) {
    s->dir = s->want; int nx = s->bx[0] + DX[s->dir], ny = s->by[0] + DY[s->dir], eat = nx == s->fx && ny == s->fy;
    int body = s->len - (eat ? 0 : 1);
    if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) { s->over = 1; }
    else for (int i = 0; i < body; i++) if (s->bx[i] == nx && s->by[i] == ny) { s->over = 1; break; }
    if (s->over) { A->vibrate(5); s->over_t = (int)A->ticks(); if (s->score > s->best) { s->best = s->score; hs_save(HS, s->best); } return; }
    if (eat && s->len < COLS * ROWS) s->len++;
    for (int i = s->len - 1; i > 0; i--) { s->bx[i] = s->bx[i - 1]; s->by[i] = s->by[i - 1]; }
    s->bx[0] = (u8)nx; s->by[0] = (u8)ny;
    if (eat) { s->score++; A->vibrate(1); if (s->ms > 80) s->ms -= 5; place_food(s); }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)tap; int g = touch_update(&s->t, pressed, x, y);
    if (s->over) { if (g == T_TAP && (int)(now - (u32)s->over_t) > 600) reset(s); }
    else {
        if (g == T_LEFT) turn(s, 3); else if (g == T_RIGHT) turn(s, 1); else if (g == T_UP) turn(s, 0); else if (g == T_DOWN) turn(s, 2);
        else if (g == T_TAP) turn(s, s->t.lx < 128 ? (s->dir + 3) & 3 : (s->dir + 1) & 3);
        if (now - s->last >= (u32)s->ms) { s->last = now; advance(s); }
    }
    clear(C_BG); title("Snake", s->score, s->best);
    A->rect(FB, 0, OY - 2, F3_W, 2, RGB(60, 70, 110)); A->rect(FB, 0, OY + ROWS * CELL, F3_W, 2, RGB(60, 70, 110));
    for (int i = s->len - 1; i >= 0; i--) A->rrect(FB, s->bx[i] * CELL + 1, OY + s->by[i] * CELL + 1, CELL - 2, CELL - 2, 4, i == 0 ? RGB(120, 240, 140) : RGB(50, 180, 90));
    if (s->fx >= 0) A->disc(FB, s->fx * CELL + CELL / 2, OY + s->fy * CELL + CELL / 2, 6, RGB(240, 80, 80));
    if (s->over) over_box(TR("Fim de jogo", "Game over"), s->score);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 7u; s->best = hs_load(HS); s->last = api->ticks(); reset(s); return &s->base;
}
