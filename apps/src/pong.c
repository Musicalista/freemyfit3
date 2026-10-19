/* Pong: drag to move your paddle (bottom). First to 7 wins; the CPU gets faster with every hit. */
#include "f3game.h"

#define PW 58
#define PY 372
#define AY 46
#define TOP 40
typedef struct { F3App base; u32 rng, last; int bx, by, vx, vy, pp, ap, ps, as, state, hits, over_t; } St;   /* positions x16; state 0 serve, 1 play, 2 over */

static void serve(St *s, int dir) { s->bx = 124 * 16; s->by = 200 * 16; s->vx = (int)((rng_next(&s->rng) & 1) ? 36 : -36) * (int)(6 + rng_next(&s->rng) % 5u) / 8; s->vy = 52 * dir; s->hits = 0; s->state = 0; s->over_t = (int)A->ticks(); }
static void reset(St *s) { s->ps = s->as = 0; s->pp = 100; s->ap = 100 * 16; serve(s, 1); }
static void step(St *s) {
    int sp = 38 + s->hits * 1; if (sp > 70) sp = 70;                                               /* CPU speed (x16/frame) */
    int tgt = s->vy < 0 ? s->bx / 16 + 4 - PW / 2 : 100; tgt = clampi(tgt, 0, 256 - PW) * 16;
    if (s->ap < tgt - 8) s->ap += sp > tgt - s->ap ? tgt - s->ap : sp; else if (s->ap > tgt + 8) s->ap -= sp > s->ap - tgt ? s->ap - tgt : sp;
    s->bx += s->vx; s->by += s->vy;
    if (s->bx < 0) { s->bx = 0; s->vx = -s->vx; } if (s->bx > 248 * 16) { s->bx = 248 * 16; s->vx = -s->vx; }
    int bx = s->bx / 16, by = s->by / 16;
    if (s->vy > 0 && by + 8 >= PY && by < PY + 8 && bx + 8 >= s->pp && bx <= s->pp + PW) {
        s->vy = -s->vy * 21 / 20; if (s->vy < -144) s->vy = -144; s->vx = (bx + 4 - (s->pp + PW / 2)) * 80 / (PW / 2); s->by = (PY - 8) * 16; s->hits++; A->vibrate(1);
    } else if (s->vy < 0 && by <= AY + 8 && by + 8 > AY && bx + 8 >= s->ap / 16 && bx <= s->ap / 16 + PW) {
        s->vy = -s->vy * 21 / 20; if (s->vy > 144) s->vy = 144; s->vx = (bx + 4 - (s->ap / 16 + PW / 2)) * 80 / (PW / 2); s->by = (AY + 8) * 16; s->hits++;
    } else if (by > F3_H) { s->as++; A->vibrate(5); if (s->as >= 7) { s->state = 2; s->over_t = (int)A->ticks(); } else serve(s, -1); }
    else if (by < TOP - 16) { s->ps++; A->vibrate(4); if (s->ps >= 7) { s->state = 2; s->over_t = (int)A->ticks(); } else serve(s, 1); }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)tap; (void)y; int sx = s->pp;
    if (pressed) s->pp = clampi(x - PW / 2, 0, 256 - PW); (void)sx;
    if (s->state == 0) { if (now - (u32)s->over_t > 900) s->state = 1; }
    else if (s->state == 1) { if (now - s->last >= 33) { s->last = now; step(s); } }
    else if (pressed && (int)(now - (u32)s->over_t) > 700) reset(s);
    clear(RGB(12, 30, 24)); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, "Pong", 2, C_TEXT);
    A->num(FB, 100, 8, s->as, 2, RGB(240, 140, 120)); A->text(FB, 124, 8, "-", 2, C_MUTED); A->num(FB, 148, 8, s->ps, 2, RGB(120, 240, 160));
    for (int yy = TOP; yy < PY; yy += 24) A->rect(FB, 124, yy, 8, 12, RGB(40, 90, 70));
    A->rrect(FB, s->ap / 16, AY - 6, PW, 12, 5, RGB(240, 120, 110)); A->rrect(FB, s->pp, PY - 2, PW, 12, 5, RGB(120, 240, 170));
    A->disc(FB, s->bx / 16 + 4, s->by / 16 + 4, 6, RGB(255, 255, 255));
    if (s->state == 2) over_box(s->ps >= 7 ? TR("Você ganhou!", "You win!") : TR("Você perdeu", "You lose"), s->ps);
    else if (s->state == 0) A->ctext(FB, 128, 250, TR("arraste a raquete", "drag the paddle"), 1, C_MUTED);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 9u; s->last = api->ticks(); reset(s); return &s->base;
}
