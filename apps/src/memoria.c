/* Memory (memoria): 16 cards, 8 pairs. Tap two cards; matching pairs stay open. Fewer moves is better. */
#include "f3game.h"

#define HS "/user/memoria.sav"
#define OX 8
#define OY 56
#define CW 56
#define CH 76
#define GP 6
typedef struct { F3App base; u32 rng; u8 card[16], st[16]; int a, b, moves, found, best, flip_t, done_t; } St;   /* st: 0 hidden 1 open 2 matched */
static const u16 CC[8] = { RGB(235, 90, 90), RGB(240, 170, 60), RGB(235, 215, 70), RGB(90, 205, 110), RGB(70, 200, 215), RGB(90, 130, 245), RGB(170, 100, 235), RGB(240, 120, 190) };

static void reset(St *s) {
    for (int i = 0; i < 16; i++) { s->card[i] = (u8)(i >> 1); s->st[i] = 0; }
    for (int i = 15; i > 0; i--) { int j = (int)(rng_next(&s->rng) % (u32)(i + 1)); u8 t = s->card[i]; s->card[i] = s->card[j]; s->card[j] = t; }
    s->a = s->b = -1; s->moves = s->found = 0; s->done_t = 0;
}
static void symbol(int k, int cx, int cy, u16 bg) {
    switch (k) {
    case 0: A->disc(FB, cx, cy, 16, RGB(255, 255, 255)); break;
    case 1: A->rect(FB, cx - 14, cy - 14, 28, 28, RGB(255, 255, 255)); break;
    case 2: for (int i = -14; i <= 14; i += 2) A->rect(FB, cx - (14 - iabs(i)), cy + i, (14 - iabs(i)) * 2 + 1, 2, RGB(255, 255, 255)); break;
    case 3: A->rect(FB, cx - 4, cy - 16, 8, 32, RGB(255, 255, 255)); A->rect(FB, cx - 16, cy - 4, 32, 8, RGB(255, 255, 255)); break;
    case 4: for (int i = 0; i < 4; i++) A->rect(FB, cx - 14 + i * 8, cy - 14, 5, 28, RGB(255, 255, 255)); break;
    case 5: A->disc(FB, cx, cy, 16, RGB(255, 255, 255)); A->disc(FB, cx, cy, 8, bg); break;
    case 6: for (int i = -14; i <= 14; i += 2) A->rect(FB, cx - (iabs(i) >> 1) - 2, cy + i, iabs(i) + 5, 2, RGB(255, 255, 255)); break;
    default: A->rect(FB, cx - 16, cy - 3, 32, 6, RGB(255, 255, 255)); A->rect(FB, cx - 3, cy - 16, 6, 32, RGB(255, 255, 255)); A->rect(FB, cx - 12, cy - 12, 8, 8, RGB(255, 255, 255)); A->rect(FB, cx + 4, cy + 4, 8, 8, RGB(255, 255, 255)); break;
    }
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed;
    if (s->a >= 0 && s->b >= 0 && (int)(now - (u32)s->flip_t) > 750) {
        if (s->card[s->a] == s->card[s->b]) { s->st[s->a] = s->st[s->b] = 2; s->found++; A->vibrate(1); if (s->found == 8) { s->done_t = (int)now; A->vibrate(4); if (!s->best || s->moves < s->best) { s->best = s->moves; hs_save(HS, s->best); } } }
        else { s->st[s->a] = s->st[s->b] = 0; }
        s->a = s->b = -1;
    }
    if (tap) {
        if (s->found == 8) { if ((int)(now - (u32)s->done_t) > 500) reset(s); }
        else if (s->b < 0) {
            int c = (x - OX) / (CW + GP), r = (y - OY) / (CH + GP);
            if (x >= OX && y >= OY && c < 4 && r < 4) { int i = r * 4 + c; if (s->st[i] == 0) { s->st[i] = 1; if (s->a < 0) s->a = i; else { s->b = i; s->moves++; s->flip_t = (int)now; } } }
        }
    }
    clear(C_BG); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Memória", "Memory"), 2, C_TEXT);
    A->num(FB, 150, 2, s->moves, 2, RGB(255, 220, 120)); A->text(FB, 150, 22, TR("jogadas", "moves"), 1, C_MUTED);
    for (int i = 0; i < 16; i++) {
        int cx = OX + (i & 3) * (CW + GP), cy = OY + (i >> 2) * (CH + GP);
        if (s->st[i] == 0) { A->rrect(FB, cx, cy, CW, CH, 8, RGB(60, 70, 130)); A->rrect(FB, cx + 6, cy + 6, CW - 12, CH - 12, 5, RGB(80, 92, 160)); A->ctext(FB, cx + CW / 2, cy + CH / 2 - 12, "?", 2, RGB(150, 160, 215)); }
        else { A->rrect(FB, cx, cy, CW, CH, 8, s->st[i] == 2 ? CC[s->card[i]] : RGB(60, 66, 100)); if (s->st[i] == 1) A->rrect(FB, cx + 2, cy + 2, CW - 4, CH - 4, 6, CC[s->card[i]]); symbol(s->card[i], cx + CW / 2, cy + CH / 2, CC[s->card[i]]); }
    }
    if (s->best) { A->text(FB, 30, 380, TR("melhor", "best"), 1, C_MUTED); A->num(FB, 86, 380, s->best, 1, C_MUTED); }
    if (s->found == 8) { A->rrect(FB, 28, 160, 200, 80, 14, RGB(30, 34, 60)); A->ctext(FB, 128, 172, TR("Parabéns!", "Well done!"), 2, RGB(255, 230, 110)); A->ctext(FB, 128, 204, TR("toque p/ jogar", "tap to play"), 1, C_TEXT); }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->rng = api->ticks() * 2654435761u + 17u; s->best = hs_load(HS); reset(s); return &s->base;
}
