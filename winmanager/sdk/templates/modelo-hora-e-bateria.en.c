/* Watch-face template: real time and battery. A->time and A->battery only exist on firmware v1.7 or newer (they may be NULL: always check). */
#include "f3game.h"

typedef struct { F3App base; } St;

static int tick(F3App *self, int pressed, int x, int y, int tap) {
    (void)self; (void)pressed; (void)x; (void)y; (void)tap;
    clear(RGB(10, 14, 30));
    if (!A->time) { A->ctext(FB, 128, 190, TR("firmware antigo", "old firmware"), 2, C_TEXT); return 1; }
    F3Time t; A->time(&t); char b[16];
    itoa_pad(b, t.hour, 2); A->text(FB, 8, 150, b, 7, C_TEXT);
    itoa_pad(b, t.min, 2); A->text(FB, 136, 150, b, 7, C_TEXT);
    A->rect(FB, 118, 168, 8, 8, RGB(120, 150, 255)); A->rect(FB, 118, 200, 8, 8, RGB(120, 150, 255));
    if (A->battery) { itoa_pad(b, A->battery(), 1); A->ctext(FB, 128, 280, b, 3, RGB(120, 230, 150)); A->ctext(FB, 128, 320, "%", 2, C_MUTED); }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; return &s->base;
}
