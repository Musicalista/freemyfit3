/* Tic-tac-toe (jogo da velha): you are X against the watch (O). The CPU plays well but slips now and then. Tap a square. */
#include "f3game.h"

#define OX 14
#define OY 96
#define CS 76
typedef struct { F3App base; u32 rng; u8 b[9]; int turn, result, win_line, w, l, d, end_t, first; } St;   /* b: 0 empty 1 X 2 O; result 0 playing 1 X 2 O 3 draw */
static const u8 LN[8][3] = { { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 }, { 0, 3, 6 }, { 1, 4, 7 }, { 2, 5, 8 }, { 0, 4, 8 }, { 2, 4, 6 } };

static int winner(const u8 *b, int *line) {
    for (int i = 0; i < 8; i++) if (b[LN[i][0]] && b[LN[i][0]] == b[LN[i][1]] && b[LN[i][1]] == b[LN[i][2]]) { if (line) *line = i; return b[LN[i][0]]; }
    return 0;
}
static int full(const u8 *b) { for (int i = 0; i < 9; i++) if (!b[i]) return 0; return 1; }
static int minimax(u8 *b, int who, int depth) {                                 /* score from O's point of view */
    int w = winner(b, 0); if (w == 2) return 10 - depth; if (w == 1) return depth - 10; if (full(b)) return 0;
    int best = who == 2 ? -100 : 100;
    for (int i = 0; i < 9; i++) if (!b[i]) { b[i] = (u8)who; int v = minimax(b, 3 - who, depth + 1); b[i] = 0; if (who == 2 ? v > best : v < best) best = v; }
    return best;
}
static void finish(St *s) {
    int line = -1, w = winner(s->b, &line);
    if (w) { s->result = w; s->win_line = line; if (w == 1) s->w++; else s->l++; A->vibrate(w == 1 ? 4 : 5); }
    else if (full(s->b)) { s->result = 3; s->d++; A->vibrate(1); } else return;
    s->end_t = (int)A->ticks();
}
static void cpu(St *s) {
    int best = -1000, mv = -1, slip = rng_next(&s->rng) % 100u < 12u;
    for (int i = 0; i < 9; i++) if (!s->b[i]) { s->b[i] = 2; int v = minimax(s->b, 1, 1); s->b[i] = 0; if (slip) v = (int)(rng_next(&s->rng) % 8u); if (v > best) { best = v; mv = i; } }
    if (mv >= 0) s->b[mv] = 2; finish(s);
}
static void new_round(St *s) { fill8(s->b, 0, sizeof s->b); s->result = 0; s->first ^= 1; if (s->first) cpu(s); }
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)pressed; if (tap) {
        if (s->result) { if ((int)(A->ticks() - (u32)s->end_t) > 500) new_round(s); }
        else { int c = (x - OX) / CS, r = (y - OY) / CS; if (x >= OX && y >= OY && c < 3 && r < 3 && !s->b[r * 3 + c]) { s->b[r * 3 + c] = 1; A->vibrate(1); finish(s); if (!s->result) cpu(s); } }
    }
    clear(C_BG); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Velha", "Tic-tac-toe"), 2, C_TEXT);
    A->text(FB, 20, 56, TR("você", "you"), 1, RGB(120, 200, 255)); A->num(FB, 60, 50, s->w, 2, C_TEXT); A->text(FB, 100, 56, TR("empate", "draw"), 1, C_MUTED); A->num(FB, 148, 50, s->d, 2, C_TEXT); A->text(FB, 180, 56, "cpu", 1, RGB(255, 140, 130)); A->num(FB, 212, 50, s->l, 2, C_TEXT);
    for (int i = 1; i < 3; i++) { A->rect(FB, OX + i * CS - 2, OY, 4, 3 * CS, RGB(70, 80, 130)); A->rect(FB, OX, OY + i * CS - 2, 3 * CS, 4, RGB(70, 80, 130)); }
    for (int i = 0; i < 9; i++) {
        int cx = OX + (i % 3) * CS + CS / 2, cy = OY + (i / 3) * CS + CS / 2;
        if (s->b[i] == 1) { for (int k = -22; k <= 22; k++) { A->disc(FB, cx + k, cy + k, 4, RGB(120, 200, 255)); A->disc(FB, cx + k, cy - k, 4, RGB(120, 200, 255)); } }
        else if (s->b[i] == 2) { A->disc(FB, cx, cy, 26, RGB(255, 140, 130)); A->disc(FB, cx, cy, 19, C_BG); }
    }
    if (s->result == 1 || s->result == 2) { int a = LN[s->win_line][0], b = LN[s->win_line][2]; int x1 = OX + (a % 3) * CS + CS / 2, y1 = OY + (a / 3) * CS + CS / 2, x2 = OX + (b % 3) * CS + CS / 2, y2 = OY + (b / 3) * CS + CS / 2;
        for (int k = 0; k <= 16; k++) A->disc(FB, x1 + (x2 - x1) * k / 16, y1 + (y2 - y1) * k / 16, 4, RGB(255, 230, 110)); }
    if (s->result) A->ctext(FB, 128, 350, s->result == 1 ? TR("Você ganhou!", "You win!") : s->result == 2 ? TR("CPU ganhou", "CPU wins") : TR("Deu velha", "Draw"), 2, RGB(255, 230, 110));
    if (s->result) A->ctext(FB, 128, 376, TR("toque p/ jogar", "tap to play"), 1, C_MUTED);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 13u; return &s->base;
}
