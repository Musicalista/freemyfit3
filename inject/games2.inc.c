/* ================= More small games (original code): Pong, Tic-tac-toe, Sudoku, Memory =================
 * One launcher tile ("Mais jogos") opening a 2x2 grid; one canvas, one LVGL timer, one touch handler (same shell as tools.inc.c).
 * Pong: drag to move your paddle, first to 7 wins (the CPU gets a little faster after every hit).
 * Tic-tac-toe: you are X against a minimax CPU that sometimes slips. Sudoku: generated on the watch (valid by construction), conflicts in red, hint button.
 * Memory: 4x4 cards, 8 pairs. Floats only (no software double). Needs common.inc.c (mg_num). X goes back to the grid, X on the grid back to Apps extras. */
enum { GM_GRID, GM_PONG, GM_TTT, GM_SUDOKU, GM_MEM };
typedef struct {
    u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img, *timer; int from_menu, kind;
    int pressed, x, y, dirty; u32 rng;
    float bx, by, vx, vy, ap; int pp, ps, as, pstate, hits;                                  /* pong: pstate 0 serve, 1 play, 2 over */
    u8 tb[9]; int tover, tw, tsx, tso, tsd, tfirst;                                          /* tic-tac-toe */
    u8 sol[81], puz[81], cur[81]; int ssel, slevel, shints, sdone, sconf; u32 st0, sfin;      /* sudoku */
    u8 mc[16], mo[16]; int m1, m2, mmoves, mdone; u32 mt;                                    /* memory: mo 0 hidden 1 open 2 matched */
} GM2;

static int menu_open(void *root);
static u32 gm2_rand(GM2 *t) { t->rng = t->rng * 1664525u + 1013904223u; return t->rng >> 8; }
static void gm2_x(u16 *g) { rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255)); }
static void gm2_btn(u16 *g, int x, int y, int w, int h, const char *s, int sc, u16 bg, u16 fg) { rrect(g, x, y, w, h, 10, bg); ctext(g, x + w / 2, y + (h - 12 * sc) / 2, s, sc, fg); }
static int gm2_in(GM2 *t, int x, int y, int w, int h) { return t->x >= x && t->x < x + w && t->y >= y && t->y < y + h; }

/* ---------------- grid ---------------- */
static const struct { const char *pt, *en; uint16_t bg, fg; } gm2_tiles[4] = {
    { "Pong", "Pong", C(178, 212, 246), C(48, 98, 172) }, { "Velha", "Tic-tac-toe", C(248, 190, 202), C(184, 56, 88) },
    { "Sudoku", "Sudoku", C(252, 238, 168), C(146, 120, 18) }, { "Memoria", "Memory", C(176, 228, 200), C(40, 120, 84) } };
static void gm2_tile_rect(int i, int *x, int *y, int *w, int *h) { *x = 10 + (i % 2) * 120; *y = 50 + (i / 2) * 102; *w = 116; *h = 94; }
static void gm2_icon(u16 *g, int i, int cx, int cy, uint16_t fg, uint16_t bg) {
    switch (i) {
    case 0: rrect(g, cx - 22, cy - 22, 6, 44, 3, fg); rrect(g, cx + 16, cy - 22, 6, 44, 3, fg); disc(g, cx, cy, 6, fg); break;
    case 1: for (int k = 1; k < 3; k++) { rect(g, cx - 22 + k * 15 - 1, cy - 22, 3, 44, fg); rect(g, cx - 22, cy - 22 + k * 15 - 1, 44, 3, fg); } disc(g, cx - 14, cy - 14, 5, fg); rect(g, cx + 6, cy + 6, 16, 3, fg); rect(g, cx + 6, cy + 20, 16, 3, fg); break;
    case 2: for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) rect(g, cx - 21 + c * 15, cy - 21 + r * 15, 12, 12, ((r + c) & 1) ? bg : fg); rrect(g, cx - 22, cy - 22, 3, 3, 1, fg); break;
    default: rrect(g, cx - 22, cy - 18, 20, 16, 4, fg); rrect(g, cx + 2, cy - 18, 20, 16, 4, bg); rrect(g, cx - 22, cy + 2, 20, 16, 4, bg); rrect(g, cx + 2, cy + 2, 20, 16, 4, fg); break;
    }
}
static void gm2_grid_draw(GM2 *t) {
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, P_BG); ptext(g, 14, 8, TR("Mais jogos", "More games"), 2, P_TEXT); gm2_x(g);
    for (int i = 0; i < 4; i++) {
        int x, y, w, h; gm2_tile_rect(i, &x, &y, &w, &h); card(g, x, y, w, h, 28, gm2_tiles[i].bg); gm2_icon(g, i, x + w / 2, y + 36, gm2_tiles[i].fg, gm2_tiles[i].bg);
        { const char *lb = TR(gm2_tiles[i].pt, gm2_tiles[i].en); int ln = 0; while (lb[ln]) ln++; if (ln > 7) ctext(g, x + w / 2, y + h - 22, lb, 1, gm2_tiles[i].fg); else ctext(g, x + w / 2, y + h - 28, lb, 2, gm2_tiles[i].fg); }
    }
}

/* ---------------- Pong ---------------- */
#define PG_TOP 32
#define PG_PY 382
#define PG_AY 44
#define PG_W 56
static void pong_serve(GM2 *t, int dir) { t->bx = 124; t->by = 200; t->vx = ((gm2_rand(t) & 1) ? 2.2f : -2.2f) * (0.6f + (float)(gm2_rand(t) % 5u) * 0.2f); t->vy = 3.2f * (float)dir; t->hits = 0; t->pstate = 0; }
static void pong_init(GM2 *t) { t->ps = t->as = 0; t->pp = 100; t->ap = 100; pong_serve(t, 1); }
static void pong_tick(GM2 *t) {
    if (t->pressed) { int p = t->x - PG_W / 2; t->pp = p < 0 ? 0 : p > 256 - PG_W ? 256 - PG_W : p; }
    if (t->pstate != 1) { t->dirty = 1; return; }
    float sp = 2.4f + (float)t->hits * 0.08f; if (sp > 4.2f) sp = 4.2f;                          /* CPU paddle speed grows with the rally */
    float tgt = t->vy < 0 ? t->bx + 4.0f - PG_W / 2 : 100.0f; if (tgt < 0) tgt = 0; if (tgt > 256 - PG_W) tgt = 256 - PG_W;
    if (t->ap < tgt - 1) t->ap += sp > tgt - t->ap ? tgt - t->ap : sp; else if (t->ap > tgt + 1) t->ap -= sp > t->ap - tgt ? t->ap - tgt : sp;
    t->bx += t->vx; t->by += t->vy;
    if (t->bx < 0) { t->bx = 0; t->vx = -t->vx; } if (t->bx > 248) { t->bx = 248; t->vx = -t->vx; }
    if (t->vy > 0 && t->by + 8 >= PG_PY && t->by < PG_PY + 8 && t->bx + 8 >= t->pp && t->bx <= t->pp + PG_W) {
        t->vy = -t->vy * 1.05f; if (t->vy < -9.0f) t->vy = -9.0f; t->vx = ((t->bx + 4 - (t->pp + PG_W / 2)) / (PG_W / 2)) * 5.0f; t->by = PG_PY - 8; t->hits++; MOTOR_ONCE(1, 0);
    } else if (t->vy < 0 && t->by <= PG_AY + 8 && t->by + 8 > PG_AY && t->bx + 8 >= t->ap && t->bx <= t->ap + PG_W) {
        t->vy = -t->vy * 1.03f; if (t->vy > 9.0f) t->vy = 9.0f; t->vx = ((t->bx + 4 - (t->ap + PG_W / 2)) / (PG_W / 2)) * 5.0f; t->by = PG_AY + 8; t->hits++;
    }
    if (t->by > 402) { t->as++; MOTOR_ONCE(5, 0); if (t->as >= 7) t->pstate = 2; else pong_serve(t, 1); }
    else if (t->by < PG_TOP - 4) { t->ps++; MOTOR_ONCE(4, 0); if (t->ps >= 7) t->pstate = 2; else pong_serve(t, -1); }
    t->dirty = 1;
}
static void pong_draw(GM2 *t) {
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, C(10, 14, 24));
    for (int y = PG_TOP; y < KBD_H; y += 16) rect(g, 124, y, 8, 8, C(40, 50, 76));
    rect(g, 0, 0, KBD_W, 28, C(18, 22, 36)); gm2_x(g); mg_num(g, 8, 6, t->as, 2, C(255, 120, 120)); mg_num(g, 60, 6, t->ps, 2, C(120, 220, 140));
    rrect(g, (int)t->ap, PG_AY, PG_W, 8, 4, C(240, 110, 110)); rrect(g, t->pp, PG_PY, PG_W, 8, 4, C(120, 230, 150)); rect(g, (int)t->bx, (int)t->by, 8, 8, C(255, 255, 255));
    if (t->pstate == 0) ctext(g, 128, 220, TR("toque para sacar", "tap to serve"), 1, C(180, 190, 220));
    if (t->pstate == 2) { rrect(g, 24, 150, 208, 90, 12, C(20, 8, 8)); ctext(g, 128, 164, t->ps >= 7 ? TR("VOCE VENCEU!", "YOU WIN!") : TR("VOCE PERDEU", "YOU LOSE"), 2, t->ps >= 7 ? C(120, 230, 150) : C(255, 90, 90)); ctext(g, 128, 206, TR("toque para jogar", "tap to play"), 1, C(220, 220, 230)); }
}
static void pong_tap(GM2 *t) { if (t->pstate == 0) t->pstate = 1; else if (t->pstate == 2) pong_init(t); }

/* ---------------- Tic-tac-toe ---------------- */
static const u8 ttt_lines[8][3] = { {0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6} };
static int ttt_win(const u8 *b) { for (int i = 0; i < 8; i++) if (b[ttt_lines[i][0]] && b[ttt_lines[i][0]] == b[ttt_lines[i][1]] && b[ttt_lines[i][0]] == b[ttt_lines[i][2]]) return b[ttt_lines[i][0]]; for (int i = 0; i < 9; i++) if (!b[i]) return 0; return 3; }
static int ttt_mm(u8 *b, int who, int depth) {                                                    /* score for O (2): +10 win, -10 loss */
    int w = ttt_win(b); if (w == 2) return 10 - depth; if (w == 1) return depth - 10; if (w == 3) return 0;
    int best = who == 2 ? -100 : 100;
    for (int i = 0; i < 9; i++) if (!b[i]) { b[i] = (u8)who; int s = ttt_mm(b, who == 2 ? 1 : 2, depth + 1); b[i] = 0; if (who == 2 ? s > best : s < best) best = s; }
    return best;
}
static void ttt_cpu(GM2 *t) {
    int free_n = 0, best = -1, bs = -100; for (int i = 0; i < 9; i++) if (!t->tb[i]) free_n++;
    if (!free_n) return;
    if ((gm2_rand(t) % 100u) < 15u) { int k = (int)(gm2_rand(t) % (u32)free_n); for (int i = 0; i < 9; i++) if (!t->tb[i] && k-- == 0) { t->tb[i] = 2; return; } }      /* a slip */
    for (int i = 0; i < 9; i++) if (!t->tb[i]) { t->tb[i] = 2; int s = ttt_mm(t->tb, 1, 1); t->tb[i] = 0; if (s > bs || (s == bs && (gm2_rand(t) & 1))) { bs = s; best = i; } }
    t->tb[best] = 2;
}
static void ttt_new(GM2 *t) { for (int i = 0; i < 9; i++) t->tb[i] = 0; t->tover = 0; t->tw = 0; t->tfirst ^= 1; if (t->tfirst) ttt_cpu(t); }
static void ttt_end(GM2 *t) { int w = ttt_win(t->tb); if (w) { t->tover = 1; t->tw = w; if (w == 1) t->tsx++; else if (w == 2) t->tso++; else t->tsd++; MOTOR_ONCE(w == 1 ? 4 : 5, 0); } }
static void ttt_draw(GM2 *t) {
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, C(250, 244, 236)); rect(g, 0, 0, KBD_W, 28, C(184, 56, 88)); ptext(g, 8, 9, TR("Jogo da velha", "Tic-tac-toe"), 1, C(255, 255, 255)); gm2_x(g);
    for (int k = 1; k < 3; k++) { rect(g, 18 + k * 74 - 3, 50, 6, 222, C(120, 100, 100)); rect(g, 18, 50 + k * 74 - 3, 222, 6, C(120, 100, 100)); }
    for (int i = 0; i < 9; i++) {
        int cx = 18 + (i % 3) * 74 + 37, cy = 50 + (i / 3) * 74 + 37;
        if (t->tb[i] == 1) { for (int d = -22; d <= 22; d += 2) { disc(g, cx + d, cy + d, 4, C(40, 90, 200)); disc(g, cx + d, cy - d, 4, C(40, 90, 200)); } }
        else if (t->tb[i] == 2) { disc(g, cx, cy, 25, C(210, 60, 70)); disc(g, cx, cy, 16, C(250, 244, 236)); }
    }
    ctext(g, 128, 290, t->tover ? (t->tw == 1 ? TR("VOCE VENCEU!", "YOU WIN!") : t->tw == 2 ? TR("O CELULAR... O RELOGIO VENCEU", "THE WATCH WINS") : TR("EMPATE", "DRAW")) : TR("sua vez (X)", "your turn (X)"), 1, C(60, 40, 40));
    if (t->tover) ctext(g, 128, 312, TR("toque para jogar de novo", "tap to play again"), 1, C(120, 100, 100));
    ptext(g, 10, 350, "X", 2, C(40, 90, 200)); mg_num(g, 34, 350, t->tsx, 2, C(60, 40, 40)); ptext(g, 90, 350, "=", 2, C(120, 100, 100)); mg_num(g, 114, 350, t->tsd, 2, C(60, 40, 40)); ptext(g, 170, 350, "O", 2, C(210, 60, 70)); mg_num(g, 194, 350, t->tso, 2, C(60, 40, 40));
}
static void ttt_tap(GM2 *t) {
    if (t->tover) { ttt_new(t); t->dirty = 1; return; }
    for (int i = 0; i < 9; i++) { int x = 18 + (i % 3) * 74, y = 50 + (i / 3) * 74; if (gm2_in(t, x, y, 74, 74) && !t->tb[i]) { t->tb[i] = 1; ttt_end(t); if (!t->tover) { ttt_cpu(t); ttt_end(t); } t->dirty = 1; return; } }
}

/* ---------------- Sudoku ---------------- */
#define SD_X 11
#define SD_Y 34
#define SD_C 26
static int sd_conflict(const u8 *v, int i) {
    int d = v[i]; if (!d) return 0; int r = i / 9, c = i % 9, br = r / 3 * 3, bc = c / 3 * 3;
    for (int k = 0; k < 9; k++) { if (k != c && v[r * 9 + k] == d) return 1; if (k != r && v[k * 9 + c] == d) return 1; int rr = br + k / 3, cc = bc + k % 3; if ((rr != r || cc != c) && v[rr * 9 + cc] == d) return 1; }
    return 0;
}
static void sd_new(GM2 *t) {
    int dig[9], rm[9], cm[9], band[3] = { 0, 1, 2 };
    for (int i = 0; i < 9; i++) dig[i] = i + 1;
    for (int i = 8; i > 0; i--) { int j = (int)(gm2_rand(t) % (u32)(i + 1)), s = dig[i]; dig[i] = dig[j]; dig[j] = s; }
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 2; i > 0; i--) { int j = (int)(gm2_rand(t) % (u32)(i + 1)), s = band[i]; band[i] = band[j]; band[j] = s; }
        for (int b = 0; b < 3; b++) { int in[3] = { 0, 1, 2 }; for (int i = 2; i > 0; i--) { int j = (int)(gm2_rand(t) % (u32)(i + 1)), s = in[i]; in[i] = in[j]; in[j] = s; } for (int k = 0; k < 3; k++) (pass ? cm : rm)[b * 3 + k] = band[b] * 3 + in[k]; }
    }
    for (int r = 0; r < 9; r++) for (int c = 0; c < 9; c++) t->sol[r * 9 + c] = (u8)dig[(3 * (rm[r] % 3) + rm[r] / 3 + cm[c]) % 9];
    static const int clues[3] = { 44, 36, 30 }; int order[81]; for (int i = 0; i < 81; i++) { order[i] = i; t->puz[i] = t->sol[i]; }
    for (int i = 80; i > 0; i--) { int j = (int)(gm2_rand(t) % (u32)(i + 1)), s = order[i]; order[i] = order[j]; order[j] = s; }
    for (int i = 0; i < 81 - clues[t->slevel]; i++) t->puz[order[i]] = 0;
    for (int i = 0; i < 81; i++) t->cur[i] = t->puz[i];
    t->ssel = -1; t->shints = 0; t->sdone = 0; t->sconf = 0; t->st0 = TICK_GET(); t->sfin = 0;
}
static void sd_check(GM2 *t) {
    int full = 1, bad = 0; for (int i = 0; i < 81; i++) { if (!t->cur[i]) full = 0; else if (sd_conflict(t->cur, i)) bad = 1; }
    t->sconf = bad; if (full && !bad && !t->sdone) { t->sdone = 1; t->sfin = TICK_GET() - t->st0; MOTOR_ONCE(4, 0); }
}
static void sd_draw(GM2 *t) {
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, C(252, 250, 244)); rect(g, 0, 0, KBD_W, 28, C(146, 120, 18)); ptext(g, 8, 9, "Sudoku", 1, C(255, 255, 255)); gm2_x(g);
    { u32 s = (t->sdone ? t->sfin : TICK_GET() - t->st0) / 1000u, m = s / 60u; s %= 60u; if (m > 99) m = 99; char b[6] = { (char)('0' + m / 10), (char)('0' + m % 10), ':', (char)('0' + s / 10), (char)('0' + s % 10), 0 }; ptext(g, 100, 9, b, 1, C(255, 255, 255)); }
    int sd = t->ssel >= 0 ? t->cur[t->ssel] : 0;
    for (int i = 0; i < 81; i++) {
        int r = i / 9, c = i % 9, x = SD_X + c * SD_C, y = SD_Y + r * SD_C, sel = i == t->ssel; uint16_t bg = sel ? C(255, 226, 150) : (sd && t->cur[i] == sd) ? C(214, 232, 250) : (((r / 3 + c / 3) & 1) ? C(240, 238, 230) : C(255, 255, 255));
        rect(g, x, y, SD_C, SD_C, bg);
        if (t->cur[i]) { char s[2] = { (char)('0' + t->cur[i]), 0 }; ctext(g, x + SD_C / 2, y + 7, s, 1, sd_conflict(t->cur, i) ? C(220, 40, 40) : t->puz[i] ? C(30, 30, 40) : C(40, 90, 200)); }
    }
    for (int k = 0; k <= 9; k++) { int w = (k % 3 == 0) ? 3 : 1; rect(g, SD_X + k * SD_C - w / 2, SD_Y, w, 9 * SD_C, C(60, 60, 70)); rect(g, SD_X, SD_Y + k * SD_C - w / 2, 9 * SD_C, w, C(60, 60, 70)); }
    for (int d = 1; d <= 9; d++) { char s[2] = { (char)('0' + d), 0 }; gm2_btn(g, 6 + (d - 1) * 27, 284, 25, 40, s, 2, C(232, 226, 205), C(60, 50, 20)); }
    gm2_btn(g, 8, 334, 74, 38, TR("APAGAR", "ERASE"), 1, C(210, 100, 100), C(255, 255, 255)); gm2_btn(g, 90, 334, 74, 38, TR("DICA", "HINT"), 1, C(80, 170, 120), C(255, 255, 255));
    { static const char *const lv[2][3] = { { "FACIL", "MEDIO", "DIFICIL" }, { "EASY", "MEDIUM", "HARD" } }; gm2_btn(g, 172, 334, 78, 38, lv[ui_pt() ? 0 : 1][t->slevel], 1, C(90, 96, 140), C(255, 255, 255)); }
    if (t->sdone) ctext(g, 128, 380, TR("RESOLVIDO! toque em NOVO nivel", "SOLVED! tap the level to play again"), 1, C(40, 140, 80)); else if (t->sconf) ctext(g, 128, 380, TR("ha numeros repetidos", "some numbers repeat"), 1, C(220, 40, 40));
}
static void sd_tap(GM2 *t) {
    for (int d = 1; d <= 9; d++) if (gm2_in(t, 6 + (d - 1) * 27, 284, 25, 40)) { if (t->ssel >= 0 && !t->puz[t->ssel] && !t->sdone) { t->cur[t->ssel] = (u8)d; sd_check(t); } t->dirty = 1; return; }
    if (gm2_in(t, 8, 334, 74, 38)) { if (t->ssel >= 0 && !t->puz[t->ssel] && !t->sdone) { t->cur[t->ssel] = 0; sd_check(t); } }
    else if (gm2_in(t, 90, 334, 74, 38)) { if (t->ssel >= 0 && !t->puz[t->ssel] && !t->sdone) { t->cur[t->ssel] = t->sol[t->ssel]; t->shints++; sd_check(t); } }
    else if (gm2_in(t, 172, 334, 78, 38)) { if (t->sdone || t->shints || 1) { t->slevel = (t->slevel + 1) % 3; sd_new(t); } }
    else if (t->x >= SD_X && t->x < SD_X + 9 * SD_C && t->y >= SD_Y && t->y < SD_Y + 9 * SD_C) t->ssel = (t->y - SD_Y) / SD_C * 9 + (t->x - SD_X) / SD_C;
    t->dirty = 1;
}

/* ---------------- Memory ---------------- */
static void mem_sym(u16 *g, int s, int cx, int cy, uint16_t c) {
    switch (s) {
    case 0: disc(g, cx, cy, 15, c); break;
    case 1: disc(g, cx, cy, 15, c); disc(g, cx, cy, 8, C(255, 255, 255)); break;
    case 2: rect(g, cx - 14, cy - 14, 28, 28, c); break;
    case 3: rect(g, cx - 14, cy - 14, 28, 28, c); rect(g, cx - 8, cy - 8, 16, 16, C(255, 255, 255)); break;
    case 4: rect(g, cx - 4, cy - 16, 8, 32, c); rect(g, cx - 16, cy - 4, 32, 8, c); break;
    case 5: for (int k = -14; k <= 14; k += 2) { rect(g, cx + k - 2, cy + k - 2, 5, 5, c); rect(g, cx + k - 2, cy - k - 2, 5, 5, c); } break;
    case 6: for (int r = 0; r < 8; r++) rect(g, cx - 2 - r * 2, cy - 14 + r * 4, 5 + r * 4, 4, c); break;
    default: for (int r = -7; r <= 7; r++) { int w = (7 - (r < 0 ? -r : r)) * 2 + 2; rect(g, cx - w, cy + r * 2, w * 2, 2, c); } break;
    }
}
static void mem_new(GM2 *t) {
    for (int i = 0; i < 16; i++) { t->mc[i] = (u8)(i / 2); t->mo[i] = 0; }
    for (int i = 15; i > 0; i--) { int j = (int)(gm2_rand(t) % (u32)(i + 1)); u8 s = t->mc[i]; t->mc[i] = t->mc[j]; t->mc[j] = s; }
    t->m1 = t->m2 = -1; t->mmoves = 0; t->mdone = 0; t->mt = 0;
}
static void mem_draw(GM2 *t) {
    static const uint16_t col[8] = { C(220, 60, 60), C(60, 120, 220), C(40, 170, 90), C(230, 160, 30), C(150, 80, 200), C(30, 170, 190), C(220, 90, 150), C(120, 120, 130) };
    u16 *g = t->px; rect(g, 0, 0, KBD_W, KBD_H, C(18, 40, 30)); rect(g, 0, 0, KBD_W, 28, C(40, 120, 84)); ptext(g, 8, 9, TR("Memoria", "Memory"), 1, C(255, 255, 255)); gm2_x(g);
    for (int i = 0; i < 16; i++) {
        int x = 3 + (i % 4) * 64, y = 40 + (i / 4) * 78;
        if (t->mo[i] == 0) { rrect(g, x, y, 58, 72, 10, C(60, 100, 150)); rrect(g, x + 6, y + 6, 46, 60, 7, C(76, 124, 180)); ctext(g, x + 29, y + 30, "?", 2, C(210, 226, 245)); }
        else { rrect(g, x, y, 58, 72, 10, t->mo[i] == 2 ? C(210, 240, 220) : C(255, 255, 255)); mem_sym(g, t->mc[i], x + 29, y + 36, col[t->mc[i]]); }
    }
    ptext(g, 8, 360, TR("jogadas", "moves"), 1, C(180, 220, 200)); mg_num(g, 8 + 8 * 8, 360, t->mmoves, 1, C(255, 255, 255));
    if (t->mdone) { rrect(g, 24, 150, 208, 90, 12, C(10, 30, 20)); ctext(g, 128, 164, TR("PARABENS!", "WELL DONE!"), 2, C(120, 230, 150)); ctext(g, 128, 206, TR("toque para jogar de novo", "tap to play again"), 1, C(220, 230, 225)); }
}
static void mem_tap(GM2 *t) {
    if (t->mdone) { mem_new(t); t->dirty = 1; return; }
    if (t->m2 >= 0) return;                                                                    /* two cards are showing: wait */
    for (int i = 0; i < 16; i++) { int x = 3 + (i % 4) * 64, y = 40 + (i / 4) * 78; if (!gm2_in(t, x, y, 58, 72) || t->mo[i]) continue;
        t->mo[i] = 1;
        if (t->m1 < 0) t->m1 = i;
        else { t->m2 = i; t->mmoves++; t->mt = TICK_GET();
            if (t->mc[t->m1] == t->mc[i]) { t->mo[t->m1] = t->mo[i] = 2; t->m1 = t->m2 = -1; MOTOR_ONCE(1, 0); int all = 1; for (int k = 0; k < 16; k++) if (t->mo[k] != 2) all = 0; if (all) { t->mdone = 1; MOTOR_ONCE(4, 0); } } }
        t->dirty = 1; return; }
}
static void mem_tick(GM2 *t) { if (t->m2 >= 0 && TICK_GET() - t->mt > 800u) { t->mo[t->m1] = t->mo[t->m2] = 0; t->m1 = t->m2 = -1; t->dirty = 1; } }

/* ---------------- shell ---------------- */
static void gm2_draw(GM2 *t) { switch (t->kind) { case GM_GRID: gm2_grid_draw(t); break; case GM_PONG: pong_draw(t); break; case GM_TTT: ttt_draw(t); break; case GM_SUDOKU: sd_draw(t); break; default: mem_draw(t); break; } }
static void gm2_go(GM2 *t, int kind) {
    t->kind = kind;
    if (kind == GM_PONG) pong_init(t); else if (kind == GM_TTT) { t->tfirst = 1; ttt_new(t); } else if (kind == GM_SUDOKU) sd_new(t); else if (kind == GM_MEM) mem_new(t);
    t->dirty = 1;
}
static void gm2_tick(void *timer) {
    GM2 *t = *(GM2 **)((u8 *)timer + 0xC);
    if (t->kind == GM_PONG) pong_tick(t); else if (t->kind == GM_MEM) mem_tick(t); else if (t->kind == GM_SUDOKU && !t->sdone && TICK_GET() / 1000u != t->sfin) { t->sfin = TICK_GET() / 1000u; t->dirty = 1; }
    if (t->dirty) { t->dirty = 0; gm2_draw(t); INVALIDATE(t->img); }
}
static void gm2_event(void *e) {
    int code = EV_CODE(e), x, y; GM2 *t = (GM2 *)EV_USER(e);
    if (code == 1 || code == 2) { if (touch_read(&x, &y)) { t->pressed = 1; t->x = x; t->y = y; } return; }
    if (code == 8 || code == 3) { t->pressed = 0; return; }
    if (code != 7) return;
    if (t->x >= 216 && t->y < 28) { if (t->kind == GM_GRID) { int fm = t->from_menu; void *r = t->root; TIMER_DEL(t->timer); ADD_FLAG(t->img, 1); FREE(t->cvblk); FREE(t); if (fm) menu_open(r); } else gm2_go(t, GM_GRID); return; }
    switch (t->kind) {
    case GM_GRID: for (int i = 0; i < 4; i++) { int tx, ty, tw, th; gm2_tile_rect(i, &tx, &ty, &tw, &th); if (gm2_in(t, tx, ty, tw, th)) { gm2_go(t, i + 1); break; } } break;
    case GM_PONG: pong_tap(t); break; case GM_TTT: ttt_tap(t); break; case GM_SUDOKU: sd_tap(t); break; default: mem_tap(t); break;
    }
}
static int gm2_open(void *root, int from_menu) {
    GM2 *t = (GM2 *)MALLOC(sizeof(GM2)); if (!t) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!blk) { FREE(t); return 0; }
    for (unsigned i = 0; i < sizeof *t; i++) ((u8 *)t)[i] = 0;
    t->cvblk = blk; t->px = (u16 *)((u8 *)blk + 8); t->root = root; t->from_menu = from_menu; t->kind = GM_GRID; t->rng = TICK_GET() * 2654435761u + 31u; t->slevel = 1;
    gm2_grid_draw(t);
    t->img = make_canvas(root, t->dsc, t->px, (void *)gm2_event, t);
    t->timer = TIMER_CREATE((void *)gm2_tick, 33, t);
    return 1;
}
