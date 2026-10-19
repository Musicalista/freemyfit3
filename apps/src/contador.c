/* Tally counter: tap the big area to add one; the buttons subtract one or zero it (tap Zerar twice). The total is kept on the watch. */
#include "f3game.h"

#define SAVE "/user/contador.sav"
typedef struct { F3App base; int n, arm; u32 arm_t; } St;

static void save(St *s) { hs_save(SAVE, s->n); }
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed;
    if (s->arm && now - s->arm_t > 2500) s->arm = 0;
    if (tap) {
        if (y >= 330) {
            if (x < 128) { if (s->n > 0) { s->n--; A->vibrate(1); save(s); } }
            else if (s->arm) { s->n = 0; s->arm = 0; A->vibrate(4); save(s); } else { s->arm = 1; s->arm_t = now; A->vibrate(1); }
        } else if (y >= 40) { if (s->n < 999999) s->n++; A->vibrate(1); save(s); }
    }
    clear(RGB(16, 22, 38)); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Contador", "Counter"), 2, C_TEXT);
    int digits = 1; for (int v = s->n; v >= 10; v /= 10) digits++;
    int sc = digits <= 3 ? 10 : digits <= 4 ? 8 : digits <= 5 ? 6 : 5; A->num(FB, 128 - digits * 4 * sc, 190 - 6 * sc, s->n, sc, RGB(255, 255, 255));
    A->ctext(FB, 128, 282, TR("toque para somar", "tap to add"), 1, C_MUTED);
    A->rrect(FB, 10, 340, 112, 50, 12, RGB(60, 70, 120)); A->ctext(FB, 66, 357, "-1", 2, C_TEXT);
    A->rrect(FB, 134, 340, 112, 50, 12, s->arm ? RGB(200, 70, 70) : RGB(60, 70, 120)); A->ctext(FB, 190, 357, s->arm ? TR("Confirmar", "Confirm") : TR("Zerar", "Reset"), 1, C_TEXT);
    return 1;
}
static void close_(F3App *self) { save((St *)self); A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->n = hs_load(SAVE); return &s->base;
}
