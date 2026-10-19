/* Reaction test: tap to arm; when the screen turns green, tap as fast as you can. Tapping too early loses the round. */
#include "f3game.h"

#define HS "/user/reflexo.sav"
typedef struct { F3App base; u32 rng, t0, go_at; int state, ms, best, count; } St;   /* 0 idle, 1 wait, 2 go, 3 result, 4 too early */

static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed; (void)x; (void)y;
    if (s->state == 1 && now >= s->go_at) { s->state = 2; s->t0 = now; A->vibrate(1); }
    if (tap) {
        if (s->state == 0 || s->state == 3 || s->state == 4) { s->state = 1; s->go_at = now + 1500u + rng_next(&s->rng) % 3500u; }
        else if (s->state == 1) { s->state = 4; A->vibrate(5); }
        else if (s->state == 2) { s->ms = (int)(now - s->t0); s->state = 3; s->count++; if (!s->best || s->ms < s->best) { s->best = s->ms; hs_save(HS, s->best); } A->vibrate(4); }
    }
    u16 bg = s->state == 1 ? RGB(150, 40, 40) : s->state == 2 ? RGB(40, 170, 80) : s->state == 4 ? RGB(110, 70, 20) : RGB(24, 32, 64);
    clear(bg); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Reflexo", "Reaction"), 2, C_TEXT);
    if (s->state == 0) { A->ctext(FB, 128, 170, TR("Toque para", "Tap to"), 2, C_TEXT); A->ctext(FB, 128, 200, TR("começar", "start"), 2, C_TEXT); }
    else if (s->state == 1) A->ctext(FB, 128, 185, TR("espere o verde...", "wait for green..."), 2, RGB(255, 220, 220));
    else if (s->state == 2) A->ctext(FB, 128, 185, TR("TOQUE!", "TAP!"), 4, RGB(255, 255, 255));
    else if (s->state == 3) { A->num(FB, 128 - 3 * 8 * 5, 150, s->ms, 5, RGB(255, 255, 255)); A->ctext(FB, 128, 220, "ms", 2, RGB(200, 220, 255)); A->ctext(FB, 128, 270, TR("toque para repetir", "tap to retry"), 1, C_MUTED); }
    else { A->ctext(FB, 128, 175, TR("Cedo demais!", "Too early!"), 2, RGB(255, 220, 160)); A->ctext(FB, 128, 220, TR("toque para tentar", "tap to retry"), 1, C_MUTED); }
    if (s->best) { A->text(FB, 70, 370, TR("melhor", "best"), 1, C_MUTED); A->num(FB, 122, 366, s->best, 2, RGB(255, 220, 120)); A->text(FB, 190, 370, "ms", 1, C_MUTED); }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 31u; s->best = hs_load(HS); return &s->base;
}
