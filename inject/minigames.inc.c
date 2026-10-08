/* ================= mini games: Snake, Flappy, Tetris (original code) =================
 * One generic shell (canvas + LVGL timer + touch) and three small games. Included by fit3_apps.c after kbd.c/rd.c helpers.
 * Controls are one-finger friendly. X (top-right) closes and returns to the Apps extras menu. */
enum { MG_SNAKE, MG_FLAPPY, MG_TETRIS, MG_2048 };
#define MG_W KBD_W
#define MG_H KBD_H

typedef struct { u8 bx[256], by[256]; int head, len, dir, nd, fx, fy, score, over, step, best; } Snake;
typedef struct { float y, vy; int px[3], gy[3], score, over, started, t, best; } Flappy;
typedef struct { u8 board[20][10]; int shape, rot, nshape, x, y, score, lines, level, over, t, hold_l, hold_r, best; u16 cur; } Tetris;
typedef struct { u8 b[4][4]; int score, best, over, won, added; } G2048;
typedef struct {
    u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img, *timer; int from_menu, kind;
    int pressed, x, y, tap, sx, sy, swipe; u32 rng, frame;
    union { Snake s; Flappy f; Tetris t; G2048 g2; } g;
} MG;

static void mg_close(MG *m);
static int menu_open(void *root);
static u32 mg_rand(MG *m) { m->rng = m->rng * 1664525u + 1013904223u; return m->rng >> 8; }
static void mg_num(u16 *g, int x, int y, int v, int sc, uint16_t col) {
    char d[8]; int n = 0; if (v < 0) v = 0; do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 7);
    for (int i = 0; i < n; i++) put(g, x + i * 8 * sc, y, (uint32_t)(uint8_t)d[n - 1 - i], sc, col);
}
static int mg_numw(int v) { int n = 1; while (v >= 10) { v /= 10; n++; } return n; }
static void mg_hud(MG *m, const char *name, int score, int best) {
    u16 *g = m->px; rect(g, 0, 0, MG_W, 28, C(10, 12, 20));
    ptext(g, 4, 8, name, 1, C(255, 210, 90)); mg_num(g, 80, 4, score, 2, C(255, 255, 255));
    ptext(g, 150, 4, "REC", 1, C(130, 140, 170)); mg_num(g, 150, 15, best, 1, C(130, 140, 170));
    rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255));
}
static void mg_over(MG *m, int score) {
    u16 *g = m->px; rect(g, 24, 150, 208, 90, C(24, 0, 0)); draw_wrapped(g, "FIM DE JOGO", 52, 162, 2, 12, 1, 0, C(255, 70, 70), 0);
    ptext(g, 64, 192, "pontos", 1, C(200, 200, 200)); mg_num(g, 112, 188, score, 2, C(255, 255, 255)); draw_wrapped(g, "toque para jogar", 56, 218, 1, 20, 1, 0, C(255, 255, 255), 0);
}

/* ---------------- Snake: 16x20 grid of 16px cells below a 40px HUD ---------------- */
#define SN_COLS 16
#define SN_ROWS 20
static void snake_food(MG *m) {
    Snake *s = &m->g.s;
    for (int tries = 0; tries < 200; tries++) {
        int fx = (int)(mg_rand(m) % SN_COLS), fy = (int)(mg_rand(m) % SN_ROWS), ok = 1;
        for (int i = 0; i < s->len; i++) { int k = (s->head - i) & 255; if (s->bx[k] == fx && s->by[k] == fy) { ok = 0; break; } }
        if (ok) { s->fx = fx; s->fy = fy; return; }
    }
}
static void snake_init(MG *m) {
    Snake *s = &m->g.s; int best = s->best; for (unsigned i = 0; i < sizeof *s; i++) ((u8 *)s)[i] = 0; s->best = best;
    s->len = 3; s->dir = 1; s->nd = 1; s->head = 2; for (int i = 0; i < 3; i++) { s->bx[i] = (u8)(4 + i); s->by[i] = 10; }
    snake_food(m);
}
static void snake_tick(MG *m) {
    Snake *s = &m->g.s;
    if (m->tap) {
        if (s->over) { snake_init(m); m->tap = 0; return; }
        s->nd = (m->x < 128) ? (s->dir + 3) & 3 : (s->dir + 1) & 3; m->tap = 0;           /* tap left/right half = turn left/right */
    }
    if (s->over) return;
    int every = 9 - s->score / 4; if (every < 3) every = 3;
    if (++s->step < every) return;
    s->step = 0; s->dir = s->nd;
    int hx = s->bx[s->head & 255], hy = s->by[s->head & 255];
    if (s->dir == 0) hy--; else if (s->dir == 1) hx++; else if (s->dir == 2) hy++; else hx--;
    int hit = hx < 0 || hy < 0 || hx >= SN_COLS || hy >= SN_ROWS;
    for (int i = 0; !hit && i < s->len - 1; i++) { int k = (s->head - i) & 255; if (s->bx[k] == hx && s->by[k] == hy) hit = 1; }
    if (hit) { s->over = 1; if (s->score > s->best) s->best = s->score; MOTOR_ONCE(4, 0); return; }
    s->head = (s->head + 1) & 255; s->bx[s->head] = (u8)hx; s->by[s->head] = (u8)hy;
    if (hx == s->fx && hy == s->fy) { if (s->len < 250) s->len++; s->score++; MOTOR_ONCE(1, 0); snake_food(m); }
}
static void snake_draw(MG *m) {
    Snake *s = &m->g.s; u16 *g = m->px; rect(g, 0, 0, MG_W, MG_H, C(12, 20, 12));
    for (int y = 0; y < SN_ROWS; y++) for (int x = 0; x < SN_COLS; x++) if (((x + y) & 1) == 0) rect(g, x * 16, 40 + y * 16, 16, 16, C(16, 28, 16));
    rect(g, 0, 38, MG_W, 2, C(80, 160, 80));
    rect(g, s->fx * 16 + 3, 40 + s->fy * 16 + 3, 10, 10, C(230, 50, 50));
    for (int i = s->len - 1; i >= 0; i--) { int k = (s->head - i) & 255; rect(g, s->bx[k] * 16 + 1, 40 + s->by[k] * 16 + 1, 14, 14, i == 0 ? C(120, 255, 120) : C(40, 190, 70)); }
    mg_hud(m, "SNAKE", s->score, s->best); if (s->over) mg_over(m, s->score);
}

/* ---------------- Flappy ---------------- */
#define FL_GAP 112
#define FL_GROUND 372
static void flappy_pipe(MG *m, int i, int x) { Flappy *f = &m->g.f; f->px[i] = x; f->gy[i] = 60 + (int)(mg_rand(m) % (FL_GROUND - FL_GAP - 90)); }
static void flappy_init(MG *m) {
    Flappy *f = &m->g.f; int best = f->best; for (unsigned i = 0; i < sizeof *f; i++) ((u8 *)f)[i] = 0; f->best = best;
    f->y = 190.0f; for (int i = 0; i < 3; i++) flappy_pipe(m, i, 300 + i * 130);
}
static void flappy_tick(MG *m) {
    Flappy *f = &m->g.f;
    if (m->tap) { if (f->over) { flappy_init(m); m->tap = 0; return; } f->started = 1; f->vy = -6.2f; m->tap = 0; }
    if (f->over) return;
    f->t++;
    if (!f->started) { f->y = 190.0f + 8.0f * ((f->t & 32) ? 1.0f : -1.0f) * 0.5f; return; }
    f->vy += 0.42f; f->y += f->vy;
    for (int i = 0; i < 3; i++) {
        f->px[i] -= 3 + f->score / 10;
        if (f->px[i] < -44) { int far = f->px[0]; for (int k = 1; k < 3; k++) if (f->px[k] > far) far = f->px[k]; flappy_pipe(m, i, far + 130); }
        if (f->px[i] + 44 < 60 && f->px[i] + 44 + 3 + f->score / 10 >= 60) { f->score++; MOTOR_ONCE(1, 0); }
        if (60 + 20 > f->px[i] && 60 < f->px[i] + 44 && (f->y < (float)f->gy[i] || f->y + 20 > (float)(f->gy[i] + FL_GAP))) f->over = 1;
    }
    if (f->y < 28.0f || f->y + 20 > (float)FL_GROUND) f->over = 1;
    if (f->over) { if (f->score > f->best) f->best = f->score; MOTOR_ONCE(4, 0); }
}
static void flappy_draw(MG *m) {
    Flappy *f = &m->g.f; u16 *g = m->px; rect(g, 0, 0, MG_W, MG_H, C(110, 190, 255)); rect(g, 0, FL_GROUND, MG_W, MG_H - FL_GROUND, C(210, 180, 100)); rect(g, 0, FL_GROUND, MG_W, 6, C(90, 190, 60));
    for (int i = 0; i < 3; i++) {
        int x = f->px[i]; rect(g, x, 28, 44, f->gy[i] - 28, C(60, 170, 60)); rect(g, x - 3, f->gy[i] - 14, 50, 14, C(40, 140, 40));
        rect(g, x, f->gy[i] + FL_GAP, 44, FL_GROUND - f->gy[i] - FL_GAP, C(60, 170, 60)); rect(g, x - 3, f->gy[i] + FL_GAP, 50, 14, C(40, 140, 40));
    }
    int by = (int)f->y; rect(g, 60, by, 20, 20, C(250, 210, 40)); rect(g, 72, by + 4, 6, 6, C(255, 255, 255)); rect(g, 75, by + 6, 3, 3, C(0, 0, 0)); rect(g, 78, by + 11, 10, 5, C(240, 110, 40));
    mg_hud(m, "FLAPPY", f->score, f->best);
    if (!f->started) draw_wrapped(g, "toque para voar", 64, 120, 1, 20, 1, 0, C(255, 255, 255), 0);
    if (f->over) mg_over(m, f->score);
}

/* ---------------- Tetris ---------------- */
#define TT_CELL 18
#define TT_X 8
#define TT_Y 32
static const u16 tt_shapes[7] = { 0x0F00, 0x6600, 0x4E00, 0x6C00, 0xC600, 0x8E00, 0x2E00 };        /* I O T S Z J L as 4x4 bit grids */
static const u16 tt_cols[7] = { 0x07FF, 0xFFE0, 0xA01F, 0x07E0, 0xF800, 0x001F, 0xFD20 };
static u16 tt_rot(u16 m) { u16 r = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (m & (0x8000 >> (y * 4 + x))) r |= (u16)(0x8000 >> (x * 4 + (3 - y))); return r; }
static int tt_fits(const Tetris *t, u16 m, int px, int py) {
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (m & (0x8000 >> (y * 4 + x))) {
        int bx = px + x, by = py + y; if (bx < 0 || bx >= 10 || by >= 20) return 0; if (by >= 0 && t->board[by][bx]) return 0; }
    return 1;
}
static void tt_spawn(MG *m) {
    Tetris *t = &m->g.t; t->shape = t->nshape; t->nshape = (int)(mg_rand(m) % 7u); t->cur = tt_shapes[t->shape]; t->x = 3; t->y = -1;
    if (!tt_fits(t, t->cur, t->x, t->y)) { t->over = 1; if (t->score > t->best) t->best = t->score; MOTOR_ONCE(4, 0); }
}
static void tetris_init(MG *m) {
    Tetris *t = &m->g.t; int best = t->best; for (unsigned i = 0; i < sizeof *t; i++) ((u8 *)t)[i] = 0; t->best = best; t->level = 1; t->nshape = (int)(mg_rand(m) % 7u); tt_spawn(m);
}
static void tt_lock(MG *m) {
    Tetris *t = &m->g.t;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (t->cur & (0x8000 >> (y * 4 + x))) { int bx = t->x + x, by = t->y + y; if (by >= 0 && by < 20 && bx >= 0 && bx < 10) t->board[by][bx] = (u8)(t->shape + 1); }
    int cleared = 0;
    for (int y = 19; y >= 0; y--) {
        int full = 1; for (int x = 0; x < 10; x++) if (!t->board[y][x]) { full = 0; break; }
        if (full) { for (int k = y; k > 0; k--) for (int x = 0; x < 10; x++) t->board[k][x] = t->board[k - 1][x]; for (int x = 0; x < 10; x++) t->board[0][x] = 0; cleared++; y++; }
    }
    if (cleared) { static const int pts[5] = { 0, 100, 300, 500, 800 }; t->score += pts[cleared] * t->level; t->lines += cleared; t->level = t->lines / 10 + 1; MOTOR_ONCE(1, 0); }
    tt_spawn(m);
}
static const struct { int x, y, w, h; char label; } tt_btn[4] = { { 4, 372, 58, 30, '<' }, { 66, 372, 58, 30, 'R' }, { 128, 372, 58, 30, '>' }, { 190, 372, 62, 30, 'v' } };
static int tt_hit(int x, int y) { for (int i = 0; i < 4; i++) if (x >= tt_btn[i].x && x < tt_btn[i].x + tt_btn[i].w && y >= tt_btn[i].y && y < tt_btn[i].y + tt_btn[i].h) return i; return -1; }
static void tetris_tick(MG *m) {
    Tetris *t = &m->g.t; int b = m->pressed ? tt_hit(m->x, m->y) : -1;
    if (m->tap) {
        int tb = tt_hit(m->x, m->y); m->tap = 0;
        if (t->over) { tetris_init(m); return; }
        if (tb == 1) { u16 r = tt_rot(t->cur); if (tt_fits(t, r, t->x, t->y)) t->cur = r; else if (tt_fits(t, r, t->x - 1, t->y)) { t->cur = r; t->x--; } else if (tt_fits(t, r, t->x + 1, t->y)) { t->cur = r; t->x++; } }
        else if (tb == 0 && tt_fits(t, t->cur, t->x - 1, t->y)) t->x--;
        else if (tb == 2 && tt_fits(t, t->cur, t->x + 1, t->y)) t->x++;
    }
    if (t->over) return;
    t->hold_l = (b == 0) ? t->hold_l + 1 : 0; t->hold_r = (b == 2) ? t->hold_r + 1 : 0;
    if (t->hold_l > 8 && (t->hold_l % 3) == 0 && tt_fits(t, t->cur, t->x - 1, t->y)) t->x--;
    if (t->hold_r > 8 && (t->hold_r % 3) == 0 && tt_fits(t, t->cur, t->x + 1, t->y)) t->x++;
    int every = 22 - t->level * 2; if (every < 3) every = 3; if (b == 3) every = 2;
    if (++t->t >= every) { t->t = 0; if (tt_fits(t, t->cur, t->x, t->y + 1)) t->y++; else tt_lock(m); }
}
static void tt_cell(u16 *g, int x, int y, u16 col) { rect(g, x, y, TT_CELL, TT_CELL, C(8, 8, 12)); rect(g, x + 1, y + 1, TT_CELL - 2, TT_CELL - 2, col); rect(g, x + 1, y + 1, TT_CELL - 2, 2, 0xFFFF & (col | 0x39E7)); }
static void tetris_draw(MG *m) {
    Tetris *t = &m->g.t; u16 *g = m->px; rect(g, 0, 0, MG_W, MG_H, C(10, 10, 16));
    rect(g, TT_X - 2, TT_Y - 2, 10 * TT_CELL + 4, 20 * TT_CELL + 4, C(60, 70, 100)); rect(g, TT_X, TT_Y, 10 * TT_CELL, 20 * TT_CELL, C(0, 0, 0));
    for (int y = 0; y < 20; y++) for (int x = 0; x < 10; x++) if (t->board[y][x]) tt_cell(g, TT_X + x * TT_CELL, TT_Y + y * TT_CELL, tt_cols[t->board[y][x] - 1]);
    if (!t->over) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (t->cur & (0x8000 >> (y * 4 + x))) { int by = t->y + y; if (by >= 0) tt_cell(g, TT_X + (t->x + x) * TT_CELL, TT_Y + by * TT_CELL, tt_cols[t->shape]); }
    ptext(g, 196, 36, "PROX", 1, C(130, 140, 170));
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (tt_shapes[t->nshape] & (0x8000 >> (y * 4 + x))) rect(g, 198 + x * 12, 50 + y * 12, 11, 11, tt_cols[t->nshape]);
    ptext(g, 196, 112, "NIVEL", 1, C(130, 140, 170)); mg_num(g, 196, 126, t->level, 2, C(255, 255, 255));
    ptext(g, 196, 156, "LINHAS", 1, C(130, 140, 170)); mg_num(g, 196, 170, t->lines, 2, C(255, 255, 255));
    for (int i = 0; i < 4; i++) { int b = m->pressed ? tt_hit(m->x, m->y) : -1; rect(g, tt_btn[i].x, tt_btn[i].y, tt_btn[i].w, tt_btn[i].h, i == b ? C(220, 160, 30) : C(40, 44, 60)); put(g, tt_btn[i].x + tt_btn[i].w / 2 - 8, tt_btn[i].y + 3, (uint32_t)(uint8_t)tt_btn[i].label, 2, C(255, 255, 255)); }
    mg_hud(m, "TETRIS", t->score, t->best); if (t->over) mg_over(m, t->score);
}


/* ---------------- 2048: swipe (or the arrow buttons) to slide and merge tiles ---------------- */
#define G2_X 7
#define G2_Y 80
#define G2_T 56
#define G2_GAP 6
static void g2_spawn(MG *m) {
    G2048 *g = &m->g.g2; int free_[16], n = 0;
    for (int i = 0; i < 16; i++) if (!g->b[i >> 2][i & 3]) free_[n++] = i;
    if (!n) return; int c = free_[mg_rand(m) % (u32)n]; g->b[c >> 2][c & 3] = (mg_rand(m) % 10u) ? 1 : 2;
}
static void g2_init(MG *m) {
    G2048 *g = &m->g.g2; int best = g->best; for (unsigned i = 0; i < sizeof *g; i++) ((u8 *)g)[i] = 0; g->best = best; g2_spawn(m); g2_spawn(m);
}
static int g2_move(MG *m, int dir) {                                   /* 0 up, 1 right, 2 down, 3 left; returns 1 if the board changed */
    G2048 *g = &m->g.g2; int moved = 0;
    for (int i = 0; i < 4; i++) {
        u8 line[4], out[4] = { 0, 0, 0, 0 }; int r[4], c[4];
        for (int k = 0; k < 4; k++) {
            if (dir == 0) { r[k] = k; c[k] = i; } else if (dir == 2) { r[k] = 3 - k; c[k] = i; } else if (dir == 3) { r[k] = i; c[k] = k; } else { r[k] = i; c[k] = 3 - k; }
            line[k] = g->b[r[k]][c[k]];
        }
        int n = 0, last_merged = 0;
        for (int k = 0; k < 4; k++) {
            if (!line[k]) continue;
            if (n > 0 && out[n - 1] == line[k] && !last_merged) { out[n - 1]++; g->score += 1 << out[n - 1]; if (out[n - 1] == 11) g->won = 1; last_merged = 1; }
            else { out[n++] = line[k]; last_merged = 0; }
        }
        for (int k = 0; k < 4; k++) { if (g->b[r[k]][c[k]] != out[k]) moved = 1; g->b[r[k]][c[k]] = out[k]; }
    }
    if (moved) { g2_spawn(m); if (g->score > g->best) g->best = g->score; MOTOR_ONCE(1, 0); }
    return moved;
}
static int g2_can_move(const G2048 *g) {
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) { if (!g->b[r][c]) return 1; if (c < 3 && g->b[r][c] == g->b[r][c + 1]) return 1; if (r < 3 && g->b[r][c] == g->b[r + 1][c]) return 1; }
    return 0;
}
static const struct { int x, y, w, h; char label; } g2_btn[4] = { { 98, 330, 60, 34, '^' }, { 98, 368, 60, 34, 'v' }, { 30, 349, 60, 34, '<' }, { 166, 349, 60, 34, '>' } };
static int g2_hit(int x, int y) { for (int i = 0; i < 4; i++) if (x >= g2_btn[i].x && x < g2_btn[i].x + g2_btn[i].w && y >= g2_btn[i].y && y < g2_btn[i].y + g2_btn[i].h) return i; return -1; }
static void g2048_tick(MG *m) {
    G2048 *g = &m->g.g2; int dir = -1;
    if (m->swipe) { dir = m->swipe - 1; m->swipe = 0; m->tap = 0; }
    else if (m->tap) { int b = g2_hit(m->x, m->y); m->tap = 0; if (g->over) { g2_init(m); return; } if (b == 0) dir = 0; else if (b == 1) dir = 2; else if (b == 2) dir = 3; else if (b == 3) dir = 1; }
    if (g->over) { if (dir >= 0) g2_init(m); return; }
    if (dir >= 0 && g2_move(m, dir) && !g2_can_move(g)) { g->over = 1; MOTOR_ONCE(4, 0); }
}
static uint16_t g2_col(int e) {
    static const uint16_t c[12] = { 0x2104, 0xEF5B, 0xEF1A, 0xF4D0, 0xF4AA, 0xF3EC, 0xF2E8, 0xEDCE, 0xEDAD, 0xED8C, 0xED6B, 0xED4A };
    return c[e > 11 ? 11 : e];
}
static void g2048_draw(MG *m) {
    G2048 *g = &m->g.g2; u16 *p = m->px; rect(p, 0, 0, MG_W, MG_H, C(30, 28, 24));
    rect(p, G2_X - 6, G2_Y - 6, 4 * G2_T + 3 * G2_GAP + 12, 4 * G2_T + 3 * G2_GAP + 12, C(120, 108, 96));
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
        int x = G2_X + c * (G2_T + G2_GAP), y = G2_Y + r * (G2_T + G2_GAP), e = g->b[r][c];
        rect(p, x, y, G2_T, G2_T, e ? g2_col(e) : C(150, 138, 124));
        if (e) { int v = 1 << e, d = mg_numw(v), sc = d >= 4 ? 1 : 2, w = d * 8 * sc; mg_num(p, x + (G2_T - w) / 2, y + (G2_T - 12 * sc) / 2, v, sc, e <= 2 ? C(110, 100, 90) : C(255, 255, 255)); }
    }
    for (int i = 0; i < 4; i++) { rect(p, g2_btn[i].x, g2_btn[i].y, g2_btn[i].w, g2_btn[i].h, C(60, 54, 48)); put(p, g2_btn[i].x + 22, g2_btn[i].y + 5, (uint32_t)(uint8_t)g2_btn[i].label, 2, C(255, 255, 255)); }
    draw_wrapped(p, "deslize ou use as setas", 40, 36, 1, 26, 1, 0, C(150, 140, 130), 0);
    mg_hud(m, "2048", g->score, g->best);
    if (g->won && !g->over) draw_wrapped(p, "2048! continue", 70, 54, 1, 20, 1, 0, C(255, 220, 90), 0);
    if (g->over) mg_over(m, g->score);
}

/* ---------------- shell ---------------- */
static void mg_draw(MG *m) { if (m->kind == MG_SNAKE) snake_draw(m); else if (m->kind == MG_FLAPPY) flappy_draw(m); else if (m->kind == MG_2048) g2048_draw(m); else tetris_draw(m); }
static void mg_tick(void *timer) {
    MG *m = *(MG **)((u8 *)timer + 0xC); m->frame++;
    if (m->kind == MG_SNAKE) snake_tick(m); else if (m->kind == MG_FLAPPY) flappy_tick(m); else if (m->kind == MG_2048) g2048_tick(m); else tetris_tick(m);
    mg_draw(m); INVALIDATE(m->img);
}
static void mg_close(MG *m) { TIMER_DEL(m->timer); ADD_FLAG(m->img, 1); FREE(m->cvblk); if (m->from_menu) menu_open(m->root); }
static void mg_event(void *e) {
    int code = EV_CODE(e), x, y; MG *m = (MG *)EV_USER(e);
    if (code == 1) { if (touch_read(&x, &y)) { m->pressed = 1; m->x = x; m->y = y; m->sx = x; m->sy = y; } return; }
    if (code == 2) { if (touch_read(&x, &y)) { m->pressed = 1; m->x = x; m->y = y; } return; }
    if (code == 8 || code == 3) {
        m->pressed = 0;
        if (m->kind == MG_2048) { int dx = m->x - m->sx, dy = m->y - m->sy, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
            if ((ax > 24 || ay > 24) && g2_hit(m->sx, m->sy) < 0) m->swipe = ax > ay ? (dx > 0 ? 2 : 4) : (dy > 0 ? 3 : 1); }     /* 1 up 2 right 3 down 4 left */
        return;
    }
    if (code != 7) return;
    if (m->x >= 216 && m->y < 28) { mg_close(m); return; }
    if (m->swipe) return;
    m->tap = 1;
}
static int mg_open(void *root, int kind, int from_menu) {
    MG *m = (MG *)MALLOC(sizeof(MG)); if (!m) return 0;
    u32 *blk = (u32 *)MALLOC(MG_W * MG_H * 2 + 8); if (!blk) { FREE(m); return 0; }
    for (unsigned i = 0; i < sizeof *m; i++) ((u8 *)m)[i] = 0;
    m->cvblk = blk; m->px = (u16 *)((u8 *)blk + 8); m->root = root; m->kind = kind; m->from_menu = from_menu; m->rng = TICK_GET() * 2654435761u + 7u;
    if (kind == MG_SNAKE) snake_init(m); else if (kind == MG_FLAPPY) flappy_init(m); else if (kind == MG_2048) g2_init(m); else tetris_init(m);
    mg_draw(m);
    m->img = make_canvas(root, m->dsc, m->px, (void *)mg_event, m);
    m->timer = TIMER_CREATE((void *)mg_tick, kind == MG_FLAPPY ? 30 : 33, m);
    return 1;
}
