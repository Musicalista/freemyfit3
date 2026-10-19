/* Breakout: drag the paddle, break all the bricks. 3 lives; tap to launch. */
#include "f3game.h"

#define HS "/user/breakout.sav"
#define BC 7
#define BR 6
#define BW 36
#define BH 16
#define OY 52
#define PY 372
#define PW 52
typedef struct { F3App base; u32 last; int pp, bx, by, vx, vy, lives, score, best, level, state, over_t; u8 br[BR][BC]; } St;   /* positions x16; state 0 ready, 1 play, 2 over */
static const u16 RC[BR] = { RGB(235, 80, 80), RGB(240, 150, 60), RGB(235, 215, 70), RGB(90, 205, 110), RGB(70, 170, 235), RGB(170, 100, 235) };

static void fill_bricks(St *s) { for (int r = 0; r < BR; r++) for (int c = 0; c < BC; c++) s->br[r][c] = 1; }
static void ready(St *s) { s->state = 0; s->bx = (s->pp + PW / 2) * 16; s->by = (PY - 10) * 16; s->vx = 0; s->vy = 0; }
static void reset(St *s) { s->lives = 3; s->score = 0; s->level = 0; fill_bricks(s); ready(s); }
static int left(St *s) { int n = 0; for (int r = 0; r < BR; r++) for (int c = 0; c < BC; c++) n += s->br[r][c]; return n; }
static void step(St *s) {
    s->bx += s->vx; s->by += s->vy; int x = s->bx / 16, y = s->by / 16;
    if (x < 5) { s->bx = 5 * 16; s->vx = -s->vx; } if (x > 251) { s->bx = 251 * 16; s->vx = -s->vx; } if (y < 40) { s->by = 40 * 16; s->vy = -s->vy; }
    x = s->bx / 16; y = s->by / 16;
    if (s->vy > 0 && y + 5 >= PY && y < PY + 10 && x >= s->pp - 4 && x <= s->pp + PW + 4) { s->vy = -iabs(s->vy); s->vx = (x - (s->pp + PW / 2)) * 90 / (PW / 2); s->by = (PY - 6) * 16; A->vibrate(1); }
    int c = x / BW, r = (y - OY) / (BH + 2);
    if (y >= OY && r >= 0 && r < BR && c >= 0 && c < BC && s->br[r][c]) { s->br[r][c] = 0; s->vy = -s->vy; s->score += (BR - r) * 10; if (s->score > s->best) s->best = s->score; A->vibrate(1);
        if (!left(s)) { s->level++; fill_bricks(s); ready(s); } }
    if (y > F3_H) { s->lives--; A->vibrate(5); if (s->lives <= 0) { s->state = 2; s->over_t = (int)A->ticks(); hs_save(HS, s->best); } else ready(s); }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)y;
    if (pressed) s->pp = clampi(x - PW / 2, 0, 256 - PW);
    if (s->state == 0) { s->bx = (s->pp + PW / 2) * 16; if (tap) { s->state = 1; s->vx = 24; s->vy = -(56 + s->level * 6); if (s->vy < -110) s->vy = -110; } }
    else if (s->state == 1) { if (now - s->last >= 33) { s->last = now; step(s); } }
    else if (tap && (int)(now - (u32)s->over_t) > 600) reset(s);
    clear(C_BG); title(TR("Tijolos", "Bricks"), s->score, s->best);
    for (int r = 0; r < BR; r++) for (int c = 0; c < BC; c++) if (s->br[r][c]) A->rrect(FB, c * BW + 2, OY + r * (BH + 2), BW - 3, BH, 3, RC[r]);
    A->rrect(FB, s->pp, PY, PW, 10, 4, RGB(220, 230, 255)); A->disc(FB, s->bx / 16, s->by / 16, 5, RGB(255, 255, 255));
    for (int i = 0; i < s->lives; i++) A->disc(FB, 12 + i * 16, 392, 5, RGB(240, 90, 100));
    if (s->state == 0) A->ctext(FB, 128, 300, TR("toque para lançar", "tap to launch"), 1, C_TEXT);
    if (s->state == 2) over_box(TR("Fim de jogo", "Game over"), s->score);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->pp = 100; s->best = hs_load(HS); s->last = api->ticks(); reset(s); return &s->base;
}
