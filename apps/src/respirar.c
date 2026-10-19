/* Guided breathing: the circle grows while you breathe in (4 s), holds (2 s) and shrinks while you breathe out (6 s). Tap to start or pause; the watch buzzes at each change. */
#include "f3game.h"

typedef struct { F3App base; u32 t0, paused_at; int running, phase, cycles, total_ms; } St;
static const int DUR[3] = { 4000, 2000, 6000 };

static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed; (void)x; (void)y;
    if (tap) { if (!s->running) { s->running = 1; s->t0 = now; s->phase = 0; A->vibrate(1); } else { s->running = 0; } }
    int r = 50, el = 0;
    if (s->running) {
        el = (int)(now - s->t0);
        if (el >= DUR[s->phase]) { s->t0 += (u32)DUR[s->phase]; el -= DUR[s->phase]; s->total_ms += DUR[s->phase]; s->phase++; if (s->phase == 3) { s->phase = 0; s->cycles++; } A->vibrate(s->phase == 1 ? 4 : 1); }
        int f = el * 100 / DUR[s->phase];
        r = s->phase == 0 ? 50 + 60 * f / 100 : s->phase == 1 ? 110 : 110 - 60 * f / 100;
    }
    clear(RGB(10, 24, 36)); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Respirar", "Breathe"), 2, C_TEXT);
    A->disc(FB, 128, 210, r + 14, RGB(20, 60, 80)); A->disc(FB, 128, 210, r, RGB(60, 170, 190)); A->disc(FB, 128, 210, r - 14 > 4 ? r - 14 : 4, RGB(120, 220, 225));
    const char *msg = !s->running ? TR("toque para começar", "tap to start") : s->phase == 0 ? TR("Inspire", "Breathe in") : s->phase == 1 ? TR("Segure", "Hold") : TR("Expire", "Breathe out");
    A->ctext(FB, 128, 340, msg, 2, RGB(235, 250, 255));
    A->text(FB, 70, 376, TR("ciclos", "cycles"), 1, C_MUTED); A->num(FB, 130, 372, s->cycles, 2, C_TEXT);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; return &s->base;
}
