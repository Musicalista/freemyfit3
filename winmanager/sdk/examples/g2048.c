/* 2048: swipe to slide the tiles; equal tiles merge. */
#include "f3game.h"

#define HS "/user/g2048.sav"
#define TS 58
#define GP 6
#define OX 3
#define OY 76
typedef struct { F3App base; Touch t; u32 rng; u16 b[4][4]; int score, best, over, won, over_t, anim; } St;
static const u16 TC[12] = { RGB(60, 66, 100), RGB(238, 228, 218), RGB(237, 224, 200), RGB(242, 177, 121), RGB(245, 149, 99), RGB(246, 124, 95), RGB(246, 94, 59), RGB(237, 207, 114), RGB(237, 204, 97), RGB(237, 200, 80), RGB(237, 197, 63), RGB(237, 194, 46) };

static void add_tile(St *s) {
    int n = 0, pos[16]; for (int i = 0; i < 16; i++) if (!s->b[i >> 2][i & 3]) pos[n++] = i;
    if (!n) return; int p = pos[rng_next(&s->rng) % (u32)n]; s->b[p >> 2][p & 3] = rng_next(&s->rng) % 10u == 0 ? 4 : 2;
}
static int can_move(St *s) {
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { if (!s->b[y][x]) return 1; if (x < 3 && s->b[y][x] == s->b[y][x + 1]) return 1; if (y < 3 && s->b[y][x] == s->b[y + 1][x]) return 1; }
    return 0;
}
static void reset(St *s) { fill8(s->b, 0, sizeof s->b); s->score = 0; s->over = s->won = 0; add_tile(s); add_tile(s); }
/* slide one line (4 values) towards index 0; returns 1 when something changed */
static int slide(u16 *l, int *gain) {
    u16 o[4]; for (int i = 0; i < 4; i++) o[i] = l[i];
    u16 t[4] = { 0, 0, 0, 0 }; int n = 0; for (int i = 0; i < 4; i++) if (l[i]) t[n++] = l[i];
    for (int i = 0; i + 1 < n; i++) if (t[i] == t[i + 1]) { t[i] = (u16)(t[i] * 2); *gain += t[i]; for (int k = i + 1; k < 3; k++) t[k] = t[k + 1]; t[3] = 0; n--; }
    int ch = 0; for (int i = 0; i < 4; i++) { l[i] = t[i]; if (l[i] != o[i]) ch = 1; }
    return ch;
}
static void move(St *s, int dir) {                                          /* dir: T_LEFT, T_RIGHT, T_UP, T_DOWN */
    int ch = 0, gain = 0;
    for (int k = 0; k < 4; k++) {
        u16 l[4];
        for (int i = 0; i < 4; i++) l[i] = dir == T_LEFT ? s->b[k][i] : dir == T_RIGHT ? s->b[k][3 - i] : dir == T_UP ? s->b[i][k] : s->b[3 - i][k];
        ch |= slide(l, &gain);
        for (int i = 0; i < 4; i++) { if (dir == T_LEFT) s->b[k][i] = l[i]; else if (dir == T_RIGHT) s->b[k][3 - i] = l[i]; else if (dir == T_UP) s->b[i][k] = l[i]; else s->b[3 - i][k] = l[i]; }
    }
    if (!ch) return;
    s->score += gain; if (gain) A->vibrate(1); add_tile(s);
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (s->b[y][x] >= 2048) s->won = 1;
    if (s->score > s->best) { s->best = s->score; }
    if (!can_move(s)) { s->over = 1; s->over_t = (int)A->ticks(); A->vibrate(5); hs_save(HS, s->best); }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)tap; int g = touch_update(&s->t, pressed, x, y);
    if (s->over) { if (g == T_TAP && (int)(A->ticks() - (u32)s->over_t) > 600) reset(s); }
    else if (g >= T_LEFT) move(s, g);
    clear(C_BG); title("2048", s->score, s->best);
    A->rrect(FB, OX - 3, OY - 3, 4 * TS + 3 * GP + 6, 4 * TS + 3 * GP + 6, 8, RGB(40, 46, 80));
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
        int v = s->b[r][c], lg = 0, tx = OX + c * (TS + GP), ty = OY + r * (TS + GP); while (v > 1) { v >>= 1; lg++; }
        A->rrect(FB, tx, ty, TS, TS, 6, TC[lg > 11 ? 11 : lg]);
        if (s->b[r][c]) { int val = s->b[r][c], sc = val < 10 ? 4 : val < 100 ? 3 : val < 1000 ? 2 : 1, digits = val < 10 ? 1 : val < 100 ? 2 : val < 1000 ? 3 : 4;
            A->num(FB, tx + (TS - digits * 8 * sc) / 2, ty + (TS - 12 * sc) / 2, val, sc, val <= 4 ? RGB(80, 70, 60) : RGB(255, 255, 255)); }
    }
    A->ctext(FB, 128, OY + 4 * TS + 3 * GP + 24, TR("deslize para juntar", "swipe to merge"), 1, C_MUTED);
    if (s->won && !s->over) A->ctext(FB, 128, 46, "2048!", 1, RGB(255, 220, 120));
    if (s->over) over_box(TR("Fim de jogo", "Game over"), s->score);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 5u; s->best = hs_load(HS); reset(s); return &s->base;
}
