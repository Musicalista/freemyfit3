/* Simon: watch the colours light up, then repeat the sequence by tapping them. One more step every round. */
#include "f3game.h"

#define HS "/user/simon.sav"
#define MAXS 60
typedef struct { F3App base; u32 rng, t0; u8 seq[MAXS]; int len, show, inp, state, lit, lit_t, score, best; } St;   /* state 0 idle, 1 showing, 2 your turn, 3 over */
static const u16 DIM[4] = { RGB(120, 36, 36), RGB(36, 110, 52), RGB(36, 60, 130), RGB(130, 110, 30) };
static const u16 BRT[4] = { RGB(255, 80, 80), RGB(80, 240, 110), RGB(90, 140, 255), RGB(255, 225, 70) };

static void next_round(St *s) { if (s->len < MAXS) s->seq[s->len++] = (u8)(rng_next(&s->rng) & 3u); s->show = 0; s->inp = 0; s->state = 1; s->t0 = A->ticks() + 600u; s->lit = -1; }
static void new_game(St *s) { s->len = 0; s->score = 0; next_round(s); }
static int quad(int x, int y) { if (y < 60 || y >= 380) return -1; return (x >= 128 ? 1 : 0) + (y >= 220 ? 2 : 0); }
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed;
    if (s->state == 1) {
        int dt = (int)(now - s->t0);
        if (dt >= 0) { int step = dt / 600, ph = dt % 600; if (step >= s->len) { s->state = 2; s->lit = -1; } else { s->lit = ph < 420 ? s->seq[step] : -1; if (ph < 40 && step == s->show) { A->vibrate(1); s->show = step + 1; } } }
    } else if (s->state == 2 && tap) {
        int q = quad(x, y);
        if (q >= 0) { s->lit = q; s->lit_t = (int)now; A->vibrate(1);
            if (q == s->seq[s->inp]) { s->inp++; if (s->inp == s->len) { s->score = s->len; if (s->score > s->best) { s->best = s->score; hs_save(HS, s->best); } A->vibrate(4); next_round(s); } }
            else { s->state = 3; s->t0 = now; A->vibrate(5); } }
    } else if (s->state == 3 && tap && now - s->t0 > 600u) new_game(s);
    else if (s->state == 0 && tap) new_game(s);
    if (s->state == 2 && s->lit >= 0 && (int)(now - (u32)s->lit_t) > 250) s->lit = -1;
    clear(C_BG); title("Simon", s->score, s->best);
    for (int q = 0; q < 4; q++) A->rrect(FB, 6 + (q & 1) * 124, 62 + (q >> 1) * 160, 120, 154, 18, q == s->lit ? BRT[q] : DIM[q]);
    if (s->state == 0) A->ctext(FB, 128, 215, TR("toque p/ começar", "tap to start"), 2, RGB(255, 255, 255));
    if (s->state == 1) A->ctext(FB, 128, 386, TR("observe...", "watch..."), 1, C_MUTED);
    if (s->state == 2) A->ctext(FB, 128, 386, TR("sua vez", "your turn"), 1, C_TEXT);
    if (s->state == 3) over_box(TR("Fim de jogo", "Game over"), s->score);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 37u; s->best = hs_load(HS); return &s->base;
}
