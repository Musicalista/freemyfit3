/* Logic tests for the game apps: compile with -DT_SUDOKU / -DT_2048 / -DT_VELHA / -DT_TETRIS (the game source is included so its internals are reachable). */
#include <stdio.h>
#include <stdlib.h>
#include "f3app.h"
static unsigned fake = 1000;
static u32 ticks_(void) { return fake += 7; }
static void vib_(int id) { (void)id; }
static int fopen_(const char *p, const char *m) { (void)p; (void)m; return -1; }
#ifdef T_SUDOKU
#include "src/sudoku.c"
int main(void) {
    F3Api api = { 0 }; api.ticks = ticks_; api.vibrate = vib_; api.lang_pt = 1; A = &api; St *s = calloc(1, sizeof(St)); int bad = 0;
    for (int seed = 1; seed <= 300; seed++) {
        s->rng = (u32)seed * 7919u; gen(s);
        for (int i = 0; i < 81; i++) s->cur[i] = s->sol[i];
        check(s); if (s->conflicts || !s->done) bad++;                      /* the full solution must be conflict-free */
        int clues = 0; for (int i = 0; i < 81; i++) { if (s->puz[i]) { clues++; if (s->puz[i] != s->sol[i]) bad++; } } if (clues != 81 - 46) bad++;
    }
    printf("sudoku: %s (%d problems)\n", bad ? "FAIL" : "ok", bad); return bad != 0;
}
#endif
#ifdef T_2048
#include "src/g2048.c"
int main(void) {
    int bad = 0, g; u16 l1[4] = { 2, 2, 2, 2 }, l2[4] = { 2, 0, 2, 4 }, l3[4] = { 4, 4, 8, 0 }, l4[4] = { 2, 4, 8, 16 }, l5[4] = { 0, 0, 0, 2 };
    g = 0; slide(l1, &g); if (l1[0] != 4 || l1[1] != 4 || l1[2] || l1[3] || g != 8) { bad++; printf("l1 %d %d %d %d g=%d\n", l1[0], l1[1], l1[2], l1[3], g); }
    g = 0; slide(l2, &g); if (l2[0] != 4 || l2[1] != 4 || l2[2] || g != 4) { bad++; printf("l2 %d %d %d %d\n", l2[0], l2[1], l2[2], l2[3]); }
    g = 0; slide(l3, &g); if (l3[0] != 8 || l3[1] != 8 || l3[2] || g != 8) { bad++; printf("l3 %d %d %d %d\n", l3[0], l3[1], l3[2], l3[3]); }
    g = 0; if (slide(l4, &g) || g) bad++;
    g = 0; if (!slide(l5, &g) || l5[0] != 2) bad++;
    F3Api api = { 0 }; api.ticks = ticks_; api.vibrate = vib_; api.fopen = fopen_; A = &api; St *s = calloc(1, sizeof(St)); s->rng = 9; reset(s);
    int total0 = 0; for (int i = 0; i < 16; i++) total0 += s->b[i >> 2][i & 3];
    for (int n = 0; n < 4000 && !s->over; n++) { int d = T_LEFT + (int)(rng_next(&s->rng) % 4u); move(s, d); }
    int sum = 0, ok = 1; for (int i = 0; i < 16; i++) { int v = s->b[i >> 2][i & 3]; sum += v; if (v & (v - 1)) ok = 0; }
    if (!ok || sum < total0) bad++;
    printf("2048: %s (%d problems, game over=%d sum=%d)\n", bad ? "FAIL" : "ok", bad, s->over, sum); return bad != 0;
}
#endif
#ifdef T_VELHA
#include "src/velha.c"
/* the CPU (O) always moves with the minimax; the opponent plays random moves: the CPU must never lose when it never slips (all squares tried) */
int main(void) {
    F3Api api = { 0 }; api.ticks = ticks_; api.vibrate = vib_; A = &api; int cpu_losses = 0, games = 0; u32 r = 123;
    for (int g = 0; g < 400; g++) {
        u8 b[9] = { 0 }; int who = g & 1 ? 1 : 2;
        for (int n = 0; n < 9 && !winner(b, 0); n++) {
            if (who == 2) { int best = -1000, mv = -1; for (int i = 0; i < 9; i++) if (!b[i]) { b[i] = 2; int v = minimax(b, 1, 1); b[i] = 0; if (v > best) { best = v; mv = i; } } b[mv] = 2; }
            else { int pick; do { pick = (int)(rng_next(&r) % 9u); } while (b[pick]); b[pick] = 1; }
            who = 3 - who;
        }
        games++; if (winner(b, 0) == 1) cpu_losses++;
    }
    printf("velha: %s (%d CPU losses in %d games)\n", cpu_losses ? "FAIL" : "ok", cpu_losses, games); return cpu_losses != 0;
}
#endif
#ifdef T_TETRIS
#include "src/tetris.c"
int main(void) {
    F3Api api = { 0 }; api.ticks = ticks_; api.vibrate = vib_; api.fopen = (int (*)(const char *, const char *))0; A = &api; int bad = 0;
    /* every piece/rotation keeps exactly 4 cells inside its 4x4 box */
    for (int k = 0; k < 7; k++) for (int r = 0; r < 4; r++) { int n = 0; for (int i = 0; i < 16; i++) if (SH[k][r] & (0x8000 >> i)) n++; if (n != 4) bad++; }
    St *s = calloc(1, sizeof(St)); s->rng = 5; fill8(s->b, 0, sizeof s->b);
    for (int x = 0; x < W; x++) if (x != 4) { s->b[H - 1][x] = 1; s->b[H - 2][x] = 1; }                       /* two almost-full rows with a hole in column 4 */
    s->pk = 0; s->pr = 1; s->px = 2; s->py = H - 4; s->nk = 1; s->level = 0; lock(s);                              /* vertical I (rotation 1 = column 2 of the box) lands in column 4 */
    if (s->lines != 2) { bad++; printf("lines=%d\n", s->lines); }
    int filled = 0; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (s->b[y][x]) filled++;
    if (filled != 2) { bad++; printf("filled=%d\n", filled); }                                                  /* 4 I cells - 2 cleared rows' cells leaves the 2 above */
    printf("tetris: %s (%d problems)\n", bad ? "FAIL" : "ok", bad); return bad != 0;
}
#endif
#ifdef T_MINES
#include "src/campomin.c"
int main(void) {
    F3Api api = { 0 }; api.ticks = ticks_; api.vibrate = vib_; A = &api; int bad = 0; St *s = calloc(1, sizeof(St));
    for (int seed = 1; seed <= 500; seed++) {
        s->rng = (u32)seed * 104729u; reset(s); int safe = (seed * 7) % (COLS * ROWS); plant(s, safe); int m = 0;
        for (int i = 0; i < COLS * ROWS; i++) { m += s->mine[i]; if (iabs(i % COLS - safe % COLS) < 2 && iabs(i / COLS - safe / COLS) < 2 && s->mine[i]) bad++; }
        if (m != MINES) bad++;
        for (int i = 0; i < COLS * ROWS; i++) { int c = 0; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int nx = i % COLS + dx, ny = i / COLS + dy; if ((dx || dy) && nx >= 0 && ny >= 0 && nx < COLS && ny < ROWS) c += s->mine[ny * COLS + nx]; } if (c != s->num[i]) bad++; }
        open_cell(s, safe); if (s->mine[safe] || s->st[safe] != 1 || s->opened < 1) bad++;
        for (int i = 0; i < COLS * ROWS; i++) if (s->st[i] == 1 && s->mine[i]) bad++;                       /* the flood fill never opens a mine */
    }
    printf("minesweeper: %s (%d problems)\n", bad ? "FAIL" : "ok", bad); return bad != 0;
}
#endif
