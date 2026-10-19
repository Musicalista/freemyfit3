/* Hello: the smallest Fit3 app. Tap anywhere to count; the colour follows the finger. */
#include "f3app.h"

static const F3Api *A;
typedef struct { F3App base; int count; int hue; } St;
static const char *const MSG[2] = { "Hello from the store!", "Olá da loja de apps!" };

static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)y;
    if (tap) { s->count++; A->vibrate(1); }
    if (pressed) s->hue = x * 31 / 256;
    A->rect(A->fb, 0, 0, F3_W, F3_H, RGB(16, 20, 36));
    A->ctext(A->fb, 128, 90, MSG[A->lang_pt], 1, RGB(255, 220, 120));
    A->disc(A->fb, 128, 200, 50 + (s->count & 7) * 3, (u16)((s->hue << 11) | (20 << 5) | (31 - s->hue)));
    A->num(A->fb, 100, 300, s->count, 4, RGB(255, 255, 255));
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    s->base.tick = tick; s->base.close = close_; s->count = 0; s->hue = 10; return &s->base;
}
