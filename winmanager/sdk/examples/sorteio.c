/* Draw (sorteio): flip a coin (heads or tails) or draw a number from 1 to N (tap the number chips to change N). */
#include "f3game.h"

typedef struct { F3App base; u32 rng, t0; int mode, flipping, heads, nmax, value, rolls; } St;
static const int NM[4] = { 6, 10, 100, 1000 };
static const u8 COS16[17] = { 16, 16, 15, 14, 12, 9, 6, 3, 0, 3, 6, 9, 12, 14, 15, 16, 16 };       /* |cos| for the coin squash */

static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed;
    if (tap) {
        if (y < 40 && x < 216) s->mode = x < 108 ? 0 : 1;
        else if (s->mode == 1 && y >= 60 && y < 100) { int k = x / 64; if (k >= 0 && k < 4) s->nmax = NM[k]; }
        else if (y >= 100) { if (s->mode == 0) { s->flipping = 1; s->t0 = now; s->heads = (int)(rng_next(&s->rng) & 1u); A->vibrate(1); } else { s->value = 1 + (int)(rng_next(&s->rng) % (u32)s->nmax); s->rolls++; A->vibrate(4); } }
    }
    int fl = 0; if (s->flipping) { fl = (int)(now - s->t0); if (fl > 900) { s->flipping = 0; A->vibrate(4); } }
    clear(C_BG); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48));
    A->rrect(FB, 6, 4, 100, 26, 8, s->mode == 0 ? RGB(66, 87, 216) : RGB(44, 50, 84)); A->ctext(FB, 56, 10, TR("Moeda", "Coin"), 1, C_TEXT);
    A->rrect(FB, 110, 4, 100, 26, 8, s->mode == 1 ? RGB(66, 87, 216) : RGB(44, 50, 84)); A->ctext(FB, 160, 10, TR("Número", "Number"), 1, C_TEXT);
    if (s->mode == 0) {
        int w = s->flipping ? 140 * COS16[(fl / 56) % 17] / 16 : 140; if (w < 6) w = 6;
        A->rrect(FB, 128 - w / 2, 120, w, 140, w / 2, RGB(240, 200, 70)); if (w > 40) { A->rrect(FB, 128 - w / 2 + 8, 128, w - 16, 124, (w - 16) / 2, RGB(220, 170, 40)); A->ctext(FB, 128, 175, s->flipping ? "" : (s->heads ? TR("CARA", "HEADS") : TR("COROA", "TAILS")), 2, RGB(120, 80, 10)); }
        A->ctext(FB, 128, 300, s->flipping ? "..." : TR("toque para girar", "tap to flip"), 1, C_MUTED);
    } else {
        for (int k = 0; k < 4; k++) { A->rrect(FB, 6 + k * 62, 60, 56, 40, 10, NM[k] == s->nmax ? RGB(66, 87, 216) : RGB(44, 50, 84)); char t[12]; itoa_pad(t, NM[k], 1); A->ctext(FB, 6 + k * 62 + 28, 74, t, 1, C_TEXT); }
        if (s->value) { char t[12]; itoa_pad(t, s->value, 1); int n = 0; while (t[n]) n++; A->text(FB, 128 - n * 4 * 9, 160, t, 9, RGB(255, 255, 255)); }
        A->ctext(FB, 128, 320, TR("toque para sortear", "tap to draw"), 1, C_MUTED);
    }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 29u; s->nmax = 100; s->heads = 1; return &s->base;
}
