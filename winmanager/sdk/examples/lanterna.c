/* Flashlight: the whole screen lights up. Tap to change colour: white, warm, red (night), green, blue. The watch's own brightness setting applies. */
#include "f3game.h"

typedef struct { F3App base; int k; u32 hint_t; } St;
static const u16 COLS5[5] = { RGB(255, 255, 255), RGB(255, 214, 150), RGB(255, 40, 30), RGB(60, 255, 90), RGB(70, 120, 255) };

static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed; (void)x; (void)y;
    if (tap) { s->k = (s->k + 1) % 5; s->hint_t = now; A->vibrate(1); }
    clear(COLS5[s->k]);
    if (now - s->hint_t < 2500u) A->ctext(FB, 128, 370, TR("toque: outra cor", "tap: next colour"), 1, s->k == 0 || s->k == 1 || s->k == 3 ? RGB(60, 60, 60) : RGB(255, 255, 255));
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->hint_t = api->ticks(); return &s->base;
}
