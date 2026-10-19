/* Xadrez: regras completas (roque, en passant, promoção automática para dama, xeque, xeque-mate, afogamento), 2 jogadores no mesmo relógio,
 * sem IA. Toque na peça para selecionar (os destinos legais ficam marcados), toque no destino para mover. "Desfazer" volta 1 lance. */
#include "f3game.h"

#define SQ 32                                   /* cell size: 8*32 = 256, matches the screen width exactly */
#define BY 40                                    /* board top */
#define EMPTY 0
/* piece values: 1 pawn 2 knight 3 bishop 4 rook 5 queen 6 king; white 1..6, black 11..16 */
static int colorOf(int v) { return v == 0 ? 0 : (v < 10 ? 1 : 2); }
static int typeOf(int v) { return v % 10; }
static int mk(int color, int type) { return color == 1 ? type : 10 + type; }

typedef struct { u8 from, to, flag; } Move;           /* flag: 0 normal, 1 double pawn push, 2 en passant capture, 3 promotion, 4 castle K, 5 castle Q */

typedef struct {
    F3App base;
    u8 board[64]; int turn;                      /* 1 white to move, 2 black */
    int sel;                                      /* selected square, -1 none */
    Move moves[220]; int nmoves;                  /* all legal moves for `turn` in the current position */
    int ep;                                       /* en passant target square, -1 none */
    int rights;                                   /* bit0 white O-O, bit1 white O-O-O, bit2 black O-O, bit3 black O-O-O */
    int over, winner;                             /* over: 0 playing, 1 checkmate, 2 stalemate */
    int msgT;                                     /* tick when the game ended, for a small "tap to continue" delay */
    u8 ub[64]; int ut, usel, uep, urights;         /* 1-level undo snapshot */
    int uhas;
    int pressed, px, py, tap;
} St;
static const int KO[8][2] = { { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 }, { 1, -2 }, { 1, 2 }, { 2, -1 }, { 2, 1 } };
static const int BD[4][2] = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
static const int RD[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

static int inb(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
static int sq(int r, int c) { return r * 8 + c; }

static int attacked(const u8 *b, int target, int by) {
    int tr = target / 8, tc = target % 8;
    for (int s = 0; s < 64; s++) {
        int v = b[s]; if (colorOf(v) != by) continue;
        int r = s / 8, c = s % 8, t = typeOf(v);
        if (t == 1) { int dir = by == 1 ? -1 : 1; if (r + dir == tr && (c - 1 == tc || c + 1 == tc)) return 1; continue; }
        if (t == 2) { for (int k = 0; k < 8; k++) if (r + KO[k][0] == tr && c + KO[k][1] == tc) return 1; continue; }
        if (t == 6) { int dr = tr - r, dc = tc - c; if (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1 && (dr || dc)) return 1; continue; }
        const int (*dirs)[2] = (t == 3) ? BD : (t == 4) ? RD : 0; int nd = 4;
        if (t == 5) { /* queen: both sets */
            int hit = 0;
            for (int d = 0; d < 4; d++) { int rr = r + BD[d][0], cc = c + BD[d][1]; while (inb(rr, cc)) { if (rr == tr && cc == tc) { hit = 1; break; } if (b[sq(rr, cc)]) break; rr += BD[d][0]; cc += BD[d][1]; } if (hit) break; }
            if (!hit) for (int d = 0; d < 4; d++) { int rr = r + RD[d][0], cc = c + RD[d][1]; while (inb(rr, cc)) { if (rr == tr && cc == tc) { hit = 1; break; } if (b[sq(rr, cc)]) break; rr += RD[d][0]; cc += RD[d][1]; } if (hit) break; }
            if (hit) return 1; continue;
        }
        if (!dirs) continue;
        for (int d = 0; d < nd; d++) { int rr = r + dirs[d][0], cc = c + dirs[d][1]; while (inb(rr, cc)) { if (rr == tr && cc == tc) return 1; if (b[sq(rr, cc)]) break; rr += dirs[d][0]; cc += dirs[d][1]; } }
    }
    return 0;
}
static int kingSq(const u8 *b, int color) { int k = mk(color, 6); for (int s = 0; s < 64; s++) if (b[s] == k) return s; return -1; }
static int inCheck(const u8 *b, int color) { return attacked(b, kingSq(b, color), 3 - color); }

/* pseudo-legal moves for `side` (no own-king-safety filter yet); appends into out, returns new count */
static int genPseudo(const u8 *b, int side, int ep, int rights, Move *out, int n) {
    for (int s = 0; s < 64; s++) {
        int v = b[s]; if (colorOf(v) != side) continue;
        int r = s / 8, c = s % 8, t = typeOf(v);
        if (t == 1) {
            int dir = side == 1 ? -1 : 1, start = side == 1 ? 6 : 1, promo = side == 1 ? 0 : 7;
            if (inb(r + dir, c) && !b[sq(r + dir, c)]) { out[n].from = (u8)s; out[n].to = (u8)sq(r + dir, c); out[n].flag = (u8)(r + dir == promo ? 3 : 0); n++;
                if (r == start && !b[sq(r + 2 * dir, c)]) { out[n].from = (u8)s; out[n].to = (u8)sq(r + 2 * dir, c); out[n].flag = 1; n++; } }
            for (int dc = -1; dc <= 1; dc += 2) if (inb(r + dir, c + dc)) {
                int ts = sq(r + dir, c + dc);
                if (b[ts] && colorOf(b[ts]) == 3 - side) { out[n].from = (u8)s; out[n].to = (u8)ts; out[n].flag = (u8)(r + dir == promo ? 3 : 0); n++; }
                else if (ts == ep) { out[n].from = (u8)s; out[n].to = (u8)ts; out[n].flag = 2; n++; }
            }
        } else if (t == 2) {
            for (int k = 0; k < 8; k++) { int rr = r + KO[k][0], cc = c + KO[k][1]; if (!inb(rr, cc)) continue; int ts = sq(rr, cc); if (!b[ts] || colorOf(b[ts]) != side) { out[n].from = (u8)s; out[n].to = (u8)ts; out[n].flag = 0; n++; } }
        } else if (t == 6) {
            for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) { if (!dr && !dc) continue; int rr = r + dr, cc = c + dc; if (!inb(rr, cc)) continue; int ts = sq(rr, cc); if (!b[ts] || colorOf(b[ts]) != side) { out[n].from = (u8)s; out[n].to = (u8)ts; out[n].flag = 0; n++; } }
            int homeRow = side == 1 ? 7 : 0;
            if (r == homeRow && c == 4 && !inCheck(b, side)) {
                int kR = side == 1 ? 1 : 4, qR = side == 1 ? 2 : 8;
                if ((rights & kR) && !b[sq(homeRow, 5)] && !b[sq(homeRow, 6)] && b[sq(homeRow, 7)] == mk(side, 4) &&
                    !attacked(b, sq(homeRow, 5), 3 - side) && !attacked(b, sq(homeRow, 6), 3 - side)) { out[n].from = (u8)s; out[n].to = (u8)sq(homeRow, 6); out[n].flag = 4; n++; }
                if ((rights & qR) && !b[sq(homeRow, 1)] && !b[sq(homeRow, 2)] && !b[sq(homeRow, 3)] && b[sq(homeRow, 0)] == mk(side, 4) &&
                    !attacked(b, sq(homeRow, 3), 3 - side) && !attacked(b, sq(homeRow, 2), 3 - side)) { out[n].from = (u8)s; out[n].to = (u8)sq(homeRow, 2); out[n].flag = 5; n++; }
            }
        } else {
            const int (*dirs)[2] = t == 3 ? BD : t == 4 ? RD : 0; int nd = 4, dirs2idx = 0;
            for (int pass = 0; pass < (t == 5 ? 2 : 1); pass++) {
                const int (*dd)[2] = t == 5 ? (pass == 0 ? BD : RD) : dirs; (void)dirs2idx;
                for (int d = 0; d < nd; d++) { int rr = r + dd[d][0], cc = c + dd[d][1];
                    while (inb(rr, cc)) { int ts = sq(rr, cc);
                        if (!b[ts]) { out[n].from = (u8)s; out[n].to = (u8)ts; out[n].flag = 0; n++; }
                        else { if (colorOf(b[ts]) != side) { out[n].from = (u8)s; out[n].to = (u8)ts; out[n].flag = 0; n++; } break; }
                        rr += dd[d][0]; cc += dd[d][1]; } }
            }
        }
    }
    return n;
}
static void rawApply(u8 *b, const Move *m) {
    int v = b[m->from]; int side = colorOf(v);
    if (m->flag == 2) { int capR = m->from / 8, capC = m->to % 8; b[sq(capR, capC)] = EMPTY; }
    if (m->flag == 4) { int row = m->from / 8; b[sq(row, 5)] = b[sq(row, 7)]; b[sq(row, 7)] = EMPTY; }
    if (m->flag == 5) { int row = m->from / 8; b[sq(row, 3)] = b[sq(row, 0)]; b[sq(row, 0)] = EMPTY; }
    b[m->to] = (u8)(m->flag == 3 ? mk(side, 5) : v); b[m->from] = EMPTY;
}
static int genLegal(const u8 *b, int side, int ep, int rights, Move *out) {
    Move pseudo[220]; int np = genPseudo(b, side, ep, rights, pseudo, 0); int n = 0;
    for (int i = 0; i < np; i++) { u8 cp[64]; for (int k = 0; k < 64; k++) cp[k] = b[k]; rawApply(cp, &pseudo[i]); if (!inCheck(cp, side)) out[n++] = pseudo[i]; }
    return n;
}
static void recompute(St *s) {
    s->nmoves = genLegal(s->board, s->turn, s->ep, s->rights, s->moves);
    if (s->nmoves == 0) { s->over = inCheck(s->board, s->turn) ? 1 : 2; s->winner = s->over == 1 ? 3 - s->turn : 0; s->msgT = (int)A->ticks(); }
}
static void newGame(St *s) {
    static const int back[8] = { 4, 2, 3, 5, 6, 3, 2, 4 };
    for (int i = 0; i < 64; i++) s->board[i] = EMPTY;
    for (int c = 0; c < 8; c++) { s->board[sq(0, c)] = (u8)mk(2, back[c]); s->board[sq(1, c)] = (u8)mk(2, 1); s->board[sq(6, c)] = (u8)mk(1, 1); s->board[sq(7, c)] = (u8)mk(1, back[c]); }
    s->turn = 1; s->sel = -1; s->ep = -1; s->rights = 1 | 2 | 4 | 8; s->over = 0; s->winner = 0; s->uhas = 0;
    recompute(s);
}
static void applyMove(St *s, const Move *m) {
    for (int k = 0; k < 64; k++) s->ub[k] = s->board[k]; s->ut = s->turn; s->usel = s->sel; s->uep = s->ep; s->urights = s->rights; s->uhas = 1;
    int v = s->board[m->from], t = typeOf(v), side = colorOf(v);
    int capture = s->board[m->to] != EMPTY || m->flag == 2;
    rawApply(s->board, m);
    s->ep = (m->flag == 1) ? (m->from + m->to) / 2 : -1;
    if (t == 6) s->rights &= side == 1 ? ~(1 | 2) : ~(4 | 8);
    if (t == 4) { if (m->from == sq(7, 7)) s->rights &= ~1; else if (m->from == sq(7, 0)) s->rights &= ~2; else if (m->from == sq(0, 7)) s->rights &= ~4; else if (m->from == sq(0, 0)) s->rights &= ~8; }
    if (m->to == sq(7, 7)) s->rights &= ~1; else if (m->to == sq(7, 0)) s->rights &= ~2; else if (m->to == sq(0, 7)) s->rights &= ~4; else if (m->to == sq(0, 0)) s->rights &= ~8;
    s->turn = 3 - s->turn; s->sel = -1; recompute(s);
    A->vibrate(s->over == 1 ? 4 : s->over == 2 ? 5 : inCheck(s->board, s->turn) ? 4 : capture ? 1 : 1);
}
static void undo(St *s) { if (!s->uhas) return; for (int k = 0; k < 64; k++) s->board[k] = s->ub[k]; s->turn = s->ut; s->sel = s->usel; s->ep = s->uep; s->rights = s->urights; s->over = 0; s->winner = 0; s->uhas = 0; recompute(s); }

static const char PL[7] = { 0, 'P', 'N', 'B', 'R', 'Q', 'K' };
static void drawBoard(St *s) {
    u16 *g = FB;
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
        int light = (r + c) % 2 == 0; u16 col = light ? RGB(236, 230, 214) : RGB(118, 92, 70);
        A->rect(g, c * SQ, BY + r * SQ, SQ, SQ, col);
    }
    if (s->over == 0 && inCheck(s->board, s->turn)) { int ks = kingSq(s->board, s->turn); int r = ks / 8, c = ks % 8; A->rect(g, c * SQ + 2, BY + r * SQ + 2, SQ - 4, SQ - 4, RGB(200, 60, 60)); }
    if (s->sel >= 0) { int r = s->sel / 8, c = s->sel % 8; A->rect(g, c * SQ + 2, BY + r * SQ + 2, SQ - 4, SQ - 4, RGB(120, 180, 255)); }
    for (int i = 0; i < s->nmoves; i++) if (s->moves[i].from == s->sel) { int r = s->moves[i].to / 8, c = s->moves[i].to % 8; A->disc(g, c * SQ + SQ / 2, BY + r * SQ + SQ / 2, 6, RGB(80, 160, 90)); }
    for (int i = 0; i < 64; i++) { int v = s->board[i]; if (!v) continue; int r = i / 8, c = i % 8; int wcol = colorOf(v) == 1; char ch[2] = { PL[typeOf(v)], 0 };
        A->disc(g, c * SQ + SQ / 2, BY + r * SQ + SQ / 2, 13, wcol ? RGB(248, 248, 250) : RGB(40, 44, 54));
        A->ctext(g, c * SQ + SQ / 2, BY + r * SQ + SQ / 2 - 6, ch, 1, wcol ? RGB(30, 30, 34) : RGB(240, 240, 244)); }
}
#define STAT_Y (BY + 8 * SQ + 6)
static void draw(St *s) {
    clear(RGB(18, 20, 28));
    A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Xadrez", "Chess"), 2, C_TEXT);
    drawBoard(s);
    if (s->over == 1) A->ctext(FB, 128, STAT_Y, s->winner == 1 ? TR("Xeque-mate: brancas venceram", "Checkmate: white wins") : TR("Xeque-mate: pretas venceram", "Checkmate: black wins"), 1, RGB(255, 210, 110));
    else if (s->over == 2) A->ctext(FB, 128, STAT_Y, TR("Afogamento: empate", "Stalemate: draw"), 1, RGB(255, 210, 110));
    else { int chk = inCheck(s->board, s->turn); A->ctext(FB, 128, STAT_Y, s->turn == 1 ? (chk ? TR("Brancas em xeque", "White in check") : TR("Vez das brancas", "White to move")) : (chk ? TR("Pretas em xeque", "Black in check") : TR("Vez das pretas", "Black to move")), 1, chk ? RGB(255, 140, 130) : C_MUTED); }
    A->rrect(FB, 14, STAT_Y + 20, 112, 34, 12, RGB(34, 40, 66)); A->ctext(FB, 70, STAT_Y + 30, TR("Novo jogo", "New game"), 1, C_TEXT);
    A->rrect(FB, 130, STAT_Y + 20, 112, 34, 12, s->uhas ? RGB(34, 40, 66) : RGB(40, 44, 54)); A->ctext(FB, 186, STAT_Y + 30, TR("Desfazer", "Undo"), 1, s->uhas ? C_TEXT : C_MUTED);
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self;
    if (tap) {
        if (s->over && (int)(A->ticks() - (u32)s->msgT) > 400) { newGame(s); }
        else if (!s->over && y >= STAT_Y + 20 && y < STAT_Y + 54) {
            if (x >= 14 && x < 126) newGame(s); else if (x >= 130 && x < 242 && s->uhas) undo(s);
        } else if (!s->over && y >= BY && y < BY + 8 * SQ && x >= 0 && x < 256) {
            int r = (y - BY) / SQ, c = x / SQ, t = sq(r, c);
            int found = 0;
            if (s->sel >= 0) for (int i = 0; i < s->nmoves; i++) if (s->moves[i].from == s->sel && s->moves[i].to == t) { applyMove(s, &s->moves[i]); found = 1; break; }
            if (!found) { if (colorOf(s->board[t]) == s->turn) s->sel = t; else s->sel = -1; }
        }
    }
    draw(s);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; newGame(s); return &s->base;
}
