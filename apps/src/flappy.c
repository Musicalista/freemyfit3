/* Flappy: tap to flap through the gaps. */
#include "f3game.h"

#define HS "/user/flappy.sav"
#define GROUND 372
#define BX 70
#define BR 11
#define GAP 118
#define PW 46
#define SP 150
typedef struct { F3App base; u32 rng, last; int y16, vy16, px[3], gy[3], scored[3], score, best, state, over_t, t; } St;   /* state 0 ready, 1 play, 2 over */

static void new_pipe(St *s, int i, int x) { s->px[i] = x; s->gy[i] = 80 + (int)(rng_next(&s->rng) % 170u); s->scored[i] = 0; }
static void reset(St *s) {
    s->y16 = 200 * 16; s->vy16 = 0; s->score = 0; s->state = 0; for (int i = 0; i < 3; i++) new_pipe(s, i, 300 + i * SP);
}
static void physics(St *s) {
    s->vy16 += 7; if (s->vy16 > 150) s->vy16 = 150; s->y16 += s->vy16;
    for (int i = 0; i < 3; i++) {
        s->px[i] -= 3; if (s->px[i] < -PW) { int far = 0; for (int k = 0; k < 3; k++) if (s->px[k] > far) far = s->px[k]; new_pipe(s, i, far + SP); }
        if (!s->scored[i] && s->px[i] + PW < BX - BR) { s->scored[i] = 1; s->score++; A->vibrate(1); }
        if (BX + BR > s->px[i] && BX - BR < s->px[i] + PW) { int y = s->y16 / 16; if (y - BR < s->gy[i] - GAP / 2 || y + BR > s->gy[i] + GAP / 2) s->state = 2; }
    }
    if (s->y16 / 16 + BR >= GROUND || s->y16 / 16 - BR < 34) s->state = 2;
    if (s->state == 2) { A->vibrate(5); s->over_t = (int)A->ticks(); if (s->score > s->best) { s->best = s->score; hs_save(HS, s->best); } }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); int down = pressed && !s->t; s->t = pressed; (void)tap; (void)x; (void)y;
    if (s->state == 0) { if (down) { s->state = 1; s->vy16 = -92; } }
    else if (s->state == 1) { if (down) s->vy16 = -92; if (now - s->last >= 33) { s->last = now; physics(s); } }
    else if (down && (int)(now - (u32)s->over_t) > 600) reset(s);
    clear(RGB(70, 170, 230)); A->rect(FB, 0, 34, F3_W, 6, RGB(60, 150, 210));
    for (int i = 0; i < 3; i++) {
        int top = s->gy[i] - GAP / 2, bot = s->gy[i] + GAP / 2;
        A->rect(FB, s->px[i], 34, PW, top - 34 > 0 ? top - 34 : 0, RGB(60, 190, 80)); A->rect(FB, s->px[i] - 3, top - 14, PW + 6, 14, RGB(40, 150, 60));
        A->rect(FB, s->px[i], bot, PW, GROUND - bot, RGB(60, 190, 80)); A->rect(FB, s->px[i] - 3, bot, PW + 6, 14, RGB(40, 150, 60));
    }
    A->rect(FB, 0, GROUND, F3_W, F3_H - GROUND, RGB(214, 190, 120)); A->rect(FB, 0, GROUND, F3_W, 4, RGB(120, 200, 70));
    A->disc(FB, BX, s->y16 / 16, BR, RGB(255, 210, 50)); A->disc(FB, BX + 5, s->y16 / 16 - 3, 3, RGB(255, 255, 255)); A->disc(FB, BX + 6, s->y16 / 16 - 3, 1, RGB(0, 0, 0)); A->rect(FB, BX + 8, s->y16 / 16 + 2, 8, 4, RGB(240, 120, 40));
    title("Flappy", s->score, s->best);
    if (s->state == 0) A->ctext(FB, 128, 280, TR("toque para voar", "tap to flap"), 2, RGB(255, 255, 255));
    if (s->state == 2) over_box(TR("Fim de jogo", "Game over"), s->score);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 3u; s->best = hs_load(HS); s->last = api->ticks(); reset(s); return &s->base;
}
