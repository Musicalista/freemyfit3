/* Paint: draw with a finger. Colour bar at the bottom (tap a colour), CLEAR at the right of the bar. */
#include "f3app.h"

static const F3Api *A;
typedef struct { F3App base; int color, lx, ly, drawing, fresh; } St;
static const u16 PAL[8] = { RGB(255, 255, 255), RGB(255, 80, 80), RGB(255, 170, 40), RGB(250, 230, 60), RGB(80, 220, 100), RGB(60, 200, 240), RGB(100, 110, 255), RGB(220, 100, 240) };
#define BAR_Y 362

static void clear(St *s) { A->rect(A->fb, 0, 0, F3_W, F3_H, RGB(10, 12, 20)); s->fresh = 1; }
static void bar(St *s) {
    A->rect(A->fb, 0, BAR_Y - 2, F3_W, F3_H - BAR_Y + 2, RGB(30, 34, 52));
    for (int i = 0; i < 8; i++) { A->rect(A->fb, 6 + i * 26, BAR_Y + 6, 22, 28, PAL[i]); if (i == s->color) A->rect(A->fb, 6 + i * 26, BAR_Y + 36, 22, 3, RGB(255, 255, 255)); }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self;
    if (s->fresh) { bar(s); s->fresh = 0; }
    if (pressed && y >= BAR_Y - 2) { if (tap || !s->drawing) { if (x >= 214) clear(s); else if (x >= 6 && x < 214) { s->color = (x - 6) / 26; if (s->color > 7) s->color = 7; bar(s); } } s->drawing = 0; return 1; }
    if (pressed && y > 30 && y < BAR_Y - 6) {
        if (s->drawing) { int dx = x - s->lx, dy = y - s->ly, n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy); if (n < 1) n = 1;
            for (int i = 0; i <= n; i++) A->disc(A->fb, s->lx + dx * i / n, s->ly + dy * i / n, 4, PAL[s->color]); }
        else A->disc(A->fb, x, y, 4, PAL[s->color]);
        s->lx = x; s->ly = y; s->drawing = 1;
    } else s->drawing = 0;
    A->rect(A->fb, 214, BAR_Y + 6, 36, 28, RGB(150, 40, 40)); A->ctext(A->fb, 232, BAR_Y + 10, A->lang_pt ? "LIMPA" : "CLR", 1, RGB(255, 255, 255));
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    s->base.tick = tick; s->base.close = close_; s->color = 1; s->drawing = 0; clear(s); return &s->base;
}
