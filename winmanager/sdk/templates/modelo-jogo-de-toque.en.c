/* Game template: tap the target before it moves. 20 hits per round; the best (shortest) time is saved in /user/.
 * Shows the essentials of a game: state, time (A->ticks), touch, drawing, vibration, game over and a best-score file. */
#include "f3game.h"

#define SAVE "/user/meujogo.sav"
typedef struct { F3App base; u32 rng, start; int tx, ty, hits, best, state; } St;   /* state 0 pronto, 1 jogando, 2 fim */

static void move_target(St *s) { s->tx = 30 + (int)(rng_next(&s->rng) % 196u); s->ty = 80 + (int)(rng_next(&s->rng) % 280u); }
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed;
    if (tap) {
        if (s->state != 1) { s->state = 1; s->hits = 0; s->start = now; move_target(s); }
        else if ((x - s->tx) * (x - s->tx) + (y - s->ty) * (y - s->ty) < 26 * 26) {
            s->hits++; A->vibrate(1); move_target(s);
            if (s->hits >= 20) { int ms = (int)(now - s->start); s->state = 2; A->vibrate(4); if (!s->best || ms < s->best) { s->best = ms; hs_save(SAVE, ms); } s->hits = ms; }
        }
    }
    clear(C_BG); title(TR("Alvo", "Target"), s->state == 1 ? s->hits : -1, 0);
    if (s->state == 1) A->disc(FB, s->tx, s->ty, 24, RGB(240, 90, 90));
    if (s->state == 0) A->ctext(FB, 128, 190, TR("toque para começar", "tap to start"), 2, C_TEXT);
    if (s->state == 2) { A->ctext(FB, 128, 150, TR("Tempo (ms)", "Time (ms)"), 2, C_TEXT); A->num(FB, 128 - 4 * 8 * 4, 190, s->hits, 4, RGB(255, 220, 120)); A->ctext(FB, 128, 260, TR("toque para repetir", "tap to retry"), 1, C_MUTED); }
    if (s->best) { A->text(FB, 40, 380, TR("recorde", "best"), 1, C_MUTED); A->num(FB, 110, 376, s->best, 2, C_TEXT); }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 1u; s->best = hs_load(SAVE); return &s->base;
}
