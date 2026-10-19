/* Minesweeper (campo minado): tap opens a square, press and hold plants a flag. The first tap is always safe. */
#include "f3game.h"

#define COLS 9
#define ROWS 12
#define CELL 28
#define OX 2
#define OY 44
#define MINES 16
typedef struct { F3App base; Press p; u32 rng; u8 mine[COLS * ROWS], num[COLS * ROWS], st[COLS * ROWS]; int started, over, won, flags, opened, over_t; } St;   /* st: 0 hidden 1 open 2 flag */
static const u16 NC[9] = { 0, RGB(90, 150, 255), RGB(80, 200, 110), RGB(255, 100, 100), RGB(180, 120, 255), RGB(255, 170, 70), RGB(70, 210, 210), RGB(240, 240, 240), RGB(160, 160, 160) };

static void reset(St *s) { fill8(s->mine, 0, sizeof s->mine); fill8(s->num, 0, sizeof s->num); fill8(s->st, 0, sizeof s->st); s->started = s->over = s->won = s->flags = s->opened = 0; }
static void plant(St *s, int safe) {
    for (int n = 0; n < MINES;) { int i = (int)(rng_next(&s->rng) % (u32)(COLS * ROWS)); int dx = iabs(i % COLS - safe % COLS), dy = iabs(i / COLS - safe / COLS); if (s->mine[i] || (dx < 2 && dy < 2)) continue; s->mine[i] = 1; n++; }
    for (int i = 0; i < COLS * ROWS; i++) { int c = 0, x = i % COLS, y = i / COLS; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int nx = x + dx, ny = y + dy; if ((dx || dy) && nx >= 0 && ny >= 0 && nx < COLS && ny < ROWS && s->mine[ny * COLS + nx]) c++; } s->num[i] = (u8)c; }
}
static void open_cell(St *s, int i0) {
    int stack[COLS * ROWS * 2], sp = 0; stack[sp++] = i0;
    while (sp) {
        int i = stack[--sp]; if (s->st[i] != 0) continue; s->st[i] = 1; s->opened++;
        if (s->num[i] == 0) { int x = i % COLS, y = i / COLS; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int nx = x + dx, ny = y + dy; if ((dx || dy) && nx >= 0 && ny >= 0 && nx < COLS && ny < ROWS && s->st[ny * COLS + nx] == 0 && sp < COLS * ROWS * 2 - 1) stack[sp++] = ny * COLS + nx; } }
    }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)tap; int k = press_update(&s->p, pressed, x, y);
    if (k && (s->over || s->won)) { if (k == 1 && (int)(A->ticks() - (u32)s->over_t) > 600) reset(s); }
    else if (k && s->p.x >= OX && s->p.y >= OY && s->p.x < OX + COLS * CELL && s->p.y < OY + ROWS * CELL) {
        int i = (s->p.y - OY) / CELL * COLS + (s->p.x - OX) / CELL;
        if (k == 2) { if (s->st[i] == 0) { s->st[i] = 2; s->flags++; A->vibrate(1); } else if (s->st[i] == 2) { s->st[i] = 0; s->flags--; A->vibrate(1); } }
        else if (s->st[i] == 0) {
            if (!s->started) { s->started = 1; plant(s, i); }
            if (s->mine[i]) { s->over = 1; s->over_t = (int)A->ticks(); A->vibrate(5); for (int j = 0; j < COLS * ROWS; j++) if (s->mine[j] && s->st[j] == 0) s->st[j] = 1; }
            else { open_cell(s, i); if (s->opened >= COLS * ROWS - MINES) { s->won = 1; s->over_t = (int)A->ticks(); A->vibrate(4); } }
        }
    }
    clear(C_BG); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Campo minado", "Minesweeper"), 1, C_TEXT);
    A->text(FB, 130, 10, TR("minas", "mines"), 1, C_MUTED); A->num(FB, 182, 4, MINES - s->flags, 2, RGB(255, 200, 100));
    for (int i = 0; i < COLS * ROWS; i++) {
        int cx = OX + (i % COLS) * CELL, cy = OY + (i / COLS) * CELL;
        if (s->st[i] == 1) {
            A->rect(FB, cx, cy, CELL - 1, CELL - 1, s->mine[i] ? RGB(120, 40, 40) : RGB(36, 42, 70));
            if (s->mine[i]) A->disc(FB, cx + CELL / 2, cy + CELL / 2, 8, RGB(20, 20, 20)); else if (s->num[i]) A->num(FB, cx + 9, cy + 7, s->num[i], 2, NC[s->num[i]]);
        } else {
            A->rrect(FB, cx + 1, cy + 1, CELL - 3, CELL - 3, 5, RGB(70, 82, 140));
            if (s->st[i] == 2) { A->rect(FB, cx + 12, cy + 6, 3, 16, RGB(230, 230, 230)); A->rect(FB, cx + 14, cy + 6, 9, 7, RGB(240, 70, 70)); }
        }
    }
    A->ctext(FB, 128, 386, TR("segure para pôr bandeira", "hold to flag"), 1, C_MUTED);
    if (s->over) over_box(TR("Boom!", "Boom!"), s->opened);
    if (s->won) { A->rrect(FB, 28, 150, 200, 90, 14, RGB(30, 34, 60)); A->ctext(FB, 128, 164, TR("Você venceu!", "You win!"), 2, RGB(255, 230, 110)); A->ctext(FB, 128, 204, TR("toque p/ jogar", "tap to play"), 1, C_TEXT); }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 23u; reset(s); return &s->base;
}
