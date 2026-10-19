/* Tetris: drag sideways to move, tap to rotate, drag down to speed up, swipe fast down to drop. */
#include "f3game.h"

#define HS "/user/tetris.sav"
#define W 10
#define H 20
#define CELL 17
#define OX 4
#define OY 44
typedef struct { F3App base; u32 rng, last; u8 b[H][W]; int px, py, pr, pk, nk, score, lines, best, over, over_t, level, ms;
    int down, sx, sy, moved, lx, ly, soft; } St;
/* 4x4 bitmaps (16 bits, row-major, MSB = top-left) for the 4 rotations of the 7 pieces I O T S Z J L */
static const u16 SH[7][4] = {
    { 0x0F00, 0x2222, 0x00F0, 0x4444 }, { 0x6600, 0x6600, 0x6600, 0x6600 }, { 0x4E00, 0x4640, 0x0E40, 0x4C40 }, { 0x6C00, 0x4620, 0x06C0, 0x8C40 },
    { 0xC600, 0x2640, 0x0C60, 0x4C80 }, { 0x8E00, 0x6440, 0x0E20, 0x44C0 }, { 0x2E00, 0x4460, 0x0E80, 0xC440 } };
static const u16 COL[7] = { RGB(60, 220, 230), RGB(240, 220, 60), RGB(180, 90, 230), RGB(90, 220, 90), RGB(235, 80, 80), RGB(80, 110, 240), RGB(245, 160, 50) };

static int fits(St *s, int x, int y, int r, int k) {
    u16 m = SH[k][r & 3];
    for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) { int cx = x + (i & 3), cy = y + (i >> 2); if (cx < 0 || cx >= W || cy >= H) return 0; if (cy >= 0 && s->b[cy][cx]) return 0; }
    return 1;
}
static void spawn(St *s) {
    s->pk = s->nk; s->nk = (int)(rng_next(&s->rng) % 7u); s->px = 3; s->py = -1; s->pr = 0;
    if (!fits(s, s->px, s->py, s->pr, s->pk)) { s->over = 1; A->vibrate(5); s->over_t = (int)A->ticks(); if (s->score > s->best) { s->best = s->score; hs_save(HS, s->best); } }
}
static void reset(St *s) { fill8(s->b, 0, sizeof s->b); s->score = s->lines = 0; s->over = 0; s->level = 0; s->ms = 600; s->nk = (int)(rng_next(&s->rng) % 7u); spawn(s); }
static void lock(St *s) {
    u16 m = SH[s->pk][s->pr & 3];
    for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) { int cx = s->px + (i & 3), cy = s->py + (i >> 2); if (cy >= 0 && cy < H && cx >= 0 && cx < W) s->b[cy][cx] = (u8)(s->pk + 1); }
    int n = 0;
    for (int y = H - 1; y >= 0; y--) {
        int full = 1; for (int x = 0; x < W; x++) if (!s->b[y][x]) { full = 0; break; }
        if (full) { n++; for (int k = y; k > 0; k--) for (int x = 0; x < W; x++) s->b[k][x] = s->b[k - 1][x]; for (int x = 0; x < W; x++) s->b[0][x] = 0; y++; }
    }
    if (n) { static const int PTS[5] = { 0, 40, 100, 300, 1200 }; s->score += PTS[n] * (s->level + 1); s->lines += n; s->level = s->lines / 10; s->ms = 600 - s->level * 50; if (s->ms < 120) s->ms = 120; A->vibrate(n > 1 ? 4 : 1); }
    spawn(s);
}
static void drop_hard(St *s) { int n = 0; while (fits(s, s->px, s->py + 1, s->pr, s->pk)) { s->py++; n++; } s->score += n; lock(s); }
static void cell(int x, int y, u16 c) { A->rrect(FB, x + 1, y + 1, CELL - 2, CELL - 2, 3, c); }
static void input(St *s, int pressed, int x, int y) {
    if (pressed) {
        if (!s->down) { s->down = 1; s->sx = x; s->sy = y; s->moved = 0; s->soft = 0; }
        s->lx = x; s->ly = y; int want = (x - s->sx) / CELL;
        while (want > s->moved && fits(s, s->px + 1, s->py, s->pr, s->pk)) { s->px++; s->moved++; }
        while (want < s->moved && fits(s, s->px - 1, s->py, s->pr, s->pk)) { s->px--; s->moved--; }
        if (want > s->moved) s->moved = want; if (want < s->moved) s->moved = want;
        s->soft = y - s->sy > 36;
    } else if (s->down) {
        s->down = 0; s->soft = 0; int dx = s->lx - s->sx, dy = s->ly - s->sy;
        if (iabs(dx) < 12 && iabs(dy) < 12) { int r = (s->pr + 1) & 3; if (fits(s, s->px, s->py, r, s->pk)) s->pr = r; else if (fits(s, s->px - 1, s->py, r, s->pk)) { s->px--; s->pr = r; } else if (fits(s, s->px + 1, s->py, r, s->pk)) { s->px++; s->pr = r; } }
        else if (dy > 70 && iabs(dx) < 40) drop_hard(s);
    }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)tap;
    if (s->over) { if (pressed && (int)(now - (u32)s->over_t) > 700) { s->down = 0; reset(s); } }
    else {
        input(s, pressed, x, y);
        if (!s->over && now - s->last >= (u32)(s->soft ? 50 : s->ms)) { s->last = now; if (fits(s, s->px, s->py + 1, s->pr, s->pk)) { s->py++; if (s->soft) s->score++; } else lock(s); }
    }
    clear(C_BG); title("Tetris", s->score, s->best);
    A->rect(FB, OX - 2, OY - 2, W * CELL + 4, H * CELL + 4, RGB(50, 58, 96)); A->rect(FB, OX, OY, W * CELL, H * CELL, RGB(8, 10, 20));
    for (int y2 = 0; y2 < H; y2++) for (int x2 = 0; x2 < W; x2++) if (s->b[y2][x2]) cell(OX + x2 * CELL, OY + y2 * CELL, COL[s->b[y2][x2] - 1]);
    if (!s->over) {
        int gy = s->py; while (fits(s, s->px, gy + 1, s->pr, s->pk)) gy++;
        u16 m = SH[s->pk][s->pr & 3];
        for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) { int cx = s->px + (i & 3), cy = s->py + (i >> 2), gyy = gy + (i >> 2);
            if (gyy >= 0) A->rect(FB, OX + cx * CELL + 3, OY + gyy * CELL + 3, CELL - 6, CELL - 6, RGB(50, 58, 96)); if (cy >= 0) cell(OX + cx * CELL, OY + cy * CELL, COL[s->pk]); }
    }
    A->text(FB, 184, 56, TR("próx", "next"), 1, C_MUTED);
    { u16 m = SH[s->nk][0]; for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) A->rrect(FB, 188 + (i & 3) * 14, 76 + (i >> 2) * 14, 12, 12, 3, COL[s->nk]); }
    A->text(FB, 184, 150, TR("linhas", "lines"), 1, C_MUTED); A->num(FB, 184, 166, s->lines, 2, C_TEXT);
    A->text(FB, 184, 200, TR("nível", "level"), 1, C_MUTED); A->num(FB, 184, 216, s->level + 1, 2, C_TEXT);
    if (s->over) over_box(TR("Fim de jogo", "Game over"), s->score);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 11u; s->best = hs_load(HS); s->last = api->ticks(); reset(s); return &s->base;
}
