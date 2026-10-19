/* Sudoku: tap a square, then a number. Wrong repeats turn red. "Novo" starts another puzzle. */
#include "f3game.h"

#define OX 2
#define OY 44
#define CS 28
typedef struct { F3App base; u32 rng, t0; u8 sol[81], puz[81], cur[81]; int sel, conflicts, done, hints; } St;

static void gen(St *s) {
    int dig[9], rows[9], cols[9];
    for (int i = 0; i < 9; i++) dig[i] = i + 1;
    for (int i = 8; i > 0; i--) { int j = (int)(rng_next(&s->rng) % (u32)(i + 1)); int t = dig[i]; dig[i] = dig[j]; dig[j] = t; }
    int band[3] = { 0, 1, 2 }, stack[3] = { 0, 1, 2 };
    for (int i = 2; i > 0; i--) { int j = (int)(rng_next(&s->rng) % (u32)(i + 1)); int t = band[i]; band[i] = band[j]; band[j] = t; j = (int)(rng_next(&s->rng) % (u32)(i + 1)); t = stack[i]; stack[i] = stack[j]; stack[j] = t; }
    for (int b = 0; b < 3; b++) { int p[3] = { 0, 1, 2 }; for (int i = 2; i > 0; i--) { int j = (int)(rng_next(&s->rng) % (u32)(i + 1)); int t = p[i]; p[i] = p[j]; p[j] = t; } for (int k = 0; k < 3; k++) rows[b * 3 + k] = band[b] * 3 + p[k]; }
    for (int b = 0; b < 3; b++) { int p[3] = { 0, 1, 2 }; for (int i = 2; i > 0; i--) { int j = (int)(rng_next(&s->rng) % (u32)(i + 1)); int t = p[i]; p[i] = p[j]; p[j] = t; } for (int k = 0; k < 3; k++) cols[b * 3 + k] = stack[b] * 3 + p[k]; }
    for (int r = 0; r < 9; r++) for (int c = 0; c < 9; c++) s->sol[r * 9 + c] = (u8)dig[(rows[r] * 3 + rows[r] / 3 + cols[c]) % 9];
    for (int i = 0; i < 81; i++) s->puz[i] = s->cur[i] = s->sol[i];
    for (int n = 0; n < 46;) { int i = (int)(rng_next(&s->rng) % 81u); if (s->puz[i]) { s->puz[i] = s->cur[i] = 0; n++; } }
    s->sel = -1; s->done = 0; s->hints = 0; s->t0 = A->ticks();
}
static int conflict(St *s, int i) {
    int v = s->cur[i]; if (!v) return 0; int r = i / 9, c = i % 9;
    for (int k = 0; k < 9; k++) { if (k != c && s->cur[r * 9 + k] == v) return 1; if (k != r && s->cur[k * 9 + c] == v) return 1; }
    for (int y = r / 3 * 3; y < r / 3 * 3 + 3; y++) for (int x = c / 3 * 3; x < c / 3 * 3 + 3; x++) if ((y != r || x != c) && s->cur[y * 9 + x] == v) return 1;
    return 0;
}
static void check(St *s) {
    s->conflicts = 0; int empty = 0; for (int i = 0; i < 81; i++) { if (!s->cur[i]) empty++; if (conflict(s, i)) s->conflicts++; }
    if (!empty && !s->conflicts && !s->done) { s->done = 1; A->vibrate(4); }
}
static void pad_rect(int k, int *x, int *y, int *w, int *h) { *w = 46; *h = 38; if (k < 5) { *x = 6 + k * 49; *y = 306; } else { *x = 6 + (k - 5) * 49; *y = 350; } }
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)pressed;
    if (tap) {
        if (s->done) gen(s);
        else if (in_rect(x, y, 146, 2, 64, 30)) gen(s);
        else if (x >= OX && y >= OY && x < OX + 9 * CS && y < OY + 9 * CS) { int i = (y - OY) / CS * 9 + (x - OX) / CS; s->sel = s->sel == i ? -1 : i; }
        else for (int k = 0; k < 10; k++) { int px, py, pw, ph; pad_rect(k, &px, &py, &pw, &ph);
            if (in_rect(x, y, px, py, pw, ph) && s->sel >= 0) {
                if (k < 9 && !s->puz[s->sel]) { s->cur[s->sel] = (u8)(k + 1); A->vibrate(1); check(s); }
                else if (k == 9 && !s->puz[s->sel]) { s->cur[s->sel] = 0; check(s); }
            } }
    }
    clear(C_BG); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, "Sudoku", 2, C_TEXT);
    A->rrect(FB, 146, 4, 64, 26, 8, RGB(60, 70, 130)); A->ctext(FB, 178, 10, TR("Novo", "New"), 1, C_TEXT);
    for (int i = 0; i < 81; i++) {
        int cx = OX + (i % 9) * CS, cy = OY + (i / 9) * CS, bad = conflict(s, i), sel = i == s->sel;
        int same = s->sel >= 0 && s->cur[s->sel] && s->cur[i] == s->cur[s->sel];
        A->rect(FB, cx, cy, CS, CS, sel ? RGB(70, 90, 170) : same ? RGB(46, 58, 110) : ((i / 27 + (i % 9) / 3) & 1) ? RGB(26, 30, 52) : RGB(34, 38, 64));
        if (s->cur[i]) A->num(FB, cx + 10, cy + 6, s->cur[i], 2, bad ? RGB(255, 100, 100) : s->puz[i] ? RGB(240, 244, 255) : RGB(120, 190, 255));
    }
    for (int k = 0; k <= 9; k++) { int th = k % 3 ? 1 : 3; A->rect(FB, OX + k * CS - th / 2, OY, th, 9 * CS, RGB(120, 130, 190)); A->rect(FB, OX, OY + k * CS - th / 2, 9 * CS, th, RGB(120, 130, 190)); }
    for (int k = 0; k < 10; k++) { int px, py, pw, ph; pad_rect(k, &px, &py, &pw, &ph); A->rrect(FB, px, py, pw, ph, 8, k == 9 ? RGB(110, 60, 70) : RGB(50, 58, 104)); if (k < 9) A->num(FB, px + 19, py + 7, k + 1, 2, C_TEXT); else A->ctext(FB, px + pw / 2, py + 12, TR("apagar", "clear"), 1, C_TEXT); }
    if (s->conflicts) A->ctext(FB, 128, 392, TR("há números repetidos", "repeated numbers"), 1, RGB(255, 110, 110));
    if (s->done) { A->rrect(FB, 28, 130, 200, 80, 14, RGB(30, 34, 60)); A->ctext(FB, 128, 142, TR("Resolvido!", "Solved!"), 2, RGB(255, 230, 110)); A->ctext(FB, 128, 176, TR("toque p/ novo", "tap for new"), 1, C_TEXT); }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 19u; gen(s); return &s->base;
}
