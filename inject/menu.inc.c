/* ================= Settings menu ("Apps extras") — pastel launcher =================
 * Opened from Settings > (tutorials entry, relabelled "Apps extras") by a one-shot LVGL timer so it lands ON TOP of the page's own widgets.
 * Samsung-style: big rounded pastel tiles in a 2x3 grid, 2 pages (1/2, 2/2) with swipe, arrows and page dots.
 * Included by fit3_apps.c after the games (uses their helpers + the primitives in kbd.c). */
typedef struct { u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img; int x, y, sx, sy, down, swiped, mode, page; } MSt;
enum { A_SNAKE, A_FLAPPY, A_TETRIS, A_2048, A_GAME, A_WEB, A_TEXT, A_HELP, A_DOOM, A_NET, A_CLOSE, A_AI, A_GB, A_TOOLS, A_DIAL, A_GAMES2, A_APPS, A_RECOVERY };
enum { I_SNAKE, I_FLAPPY, I_TETRIS, I_2048, I_AIM, I_WEB, I_TEXT, I_HELP, I_DOOM, I_CLOSE, I_NET, I_AI, I_GB, I_TOOLS, I_DIAL, I_GAMES2, I_APPS, I_RECOVERY };
static const struct { const char *pt, *en; uint16_t bg, fg; u8 icon, act; } mtile[] = {
#ifdef HAVE_MINIGAMES
    { "Snake", "Snake", C(176, 228, 200), C(40, 120, 84), I_SNAKE, A_SNAKE },   { "Flappy", "Flappy", C(178, 212, 246), C(48, 98, 172), I_FLAPPY, A_FLAPPY },
    { "Tetris", "Tetris", C(208, 190, 242), C(106, 66, 168), I_TETRIS, A_TETRIS }, { "2048", "2048", C(252, 212, 176), C(186, 106, 36), I_2048, A_2048 },
#endif
#ifndef NO_GAMES
    { GAME_LABEL, GAME_LABEL, C(248, 190, 202), C(184, 56, 88), I_AIM, A_GAME },
#endif
#if defined(NET_APP) && defined(HAVE_WEB)
    { "Internet", "Internet", C(150, 232, 200), C(24, 120, 90), I_NET, A_NET },
#ifndef NET_DIRECT_ONLY
    { "IA", "AI", C(232, 204, 250), C(140, 66, 186), I_AI, A_AI },
    { "Discador", "Dialer", C(196, 236, 190), C(40, 128, 70), I_DIAL, A_DIAL },
#endif
#endif
#if defined(HAVE_WEB) && !defined(NO_PCBRIDGE)
    { "Web", "Web", C(164, 228, 228), C(28, 126, 138), I_WEB, A_WEB },       { "Texto", "Text", C(252, 238, 168), C(146, 120, 18), I_TEXT, A_TEXT },
#endif
#ifdef HAVE_APPSYS
    { "Meus apps", "My apps", C(206, 222, 252), C(52, 84, 168), I_APPS, A_APPS },
#endif
#ifdef HAVE_GAMES2
    { "Mais jogos", "More games", C(248, 214, 190), C(176, 92, 40), I_GAMES2, A_GAMES2 },
#endif
#ifdef HAVE_TOOLS
    { "Utilitarios", "Tools", C(190, 226, 240), C(34, 100, 140), I_TOOLS, A_TOOLS },
#endif
#ifdef HAVE_GB
    { "GameBoy", "GameBoy", C(214, 232, 176), C(64, 96, 40), I_GB, A_GB },
#endif
#ifdef HAVE_RECOVERY
    { "Recuperacao", "Recovery", C(250, 208, 204), C(176, 62, 52), I_RECOVERY, A_RECOVERY },
#endif
    { "Ajuda", "Help", C(194, 204, 246), C(66, 82, 168), I_HELP, A_HELP },
#ifdef HAVE_DOOM
    { "Doom", "Doom", C(246, 176, 166), C(166, 46, 38), I_DOOM, A_DOOM },
#endif
    { "Fechar", "Close", C(226, 210, 220), C(126, 76, 98), I_CLOSE, A_CLOSE } };
#define MT_N ((int)(sizeof mtile / sizeof mtile[0]))
#define MT_PER 6
#define MT_PAGES ((MT_N + MT_PER - 1) / MT_PER)

static int menu_open(void *root);
static int menu_launch(void *root, int act) {
    switch (act) {
#ifdef HAVE_MINIGAMES
    case A_SNAKE: return mg_open(root, 0, 1);
    case A_FLAPPY: return mg_open(root, 1, 1);
    case A_TETRIS: return mg_open(root, 2, 1);
    case A_2048: return mg_open(root, 3, 1);
#endif
#ifndef NO_GAMES
    case A_GAME: return game_open(root, 1);
#endif
#ifdef HAVE_GB
    case A_GB: return gb_open(root, 1);
#endif
#ifdef HAVE_TOOLS
    case A_TOOLS: return tl_open(root, 1);
#endif
#ifdef HAVE_GAMES2
    case A_GAMES2: return gm2_open(root, 1);
#endif
#ifdef HAVE_APPSYS
    case A_APPS: return as_open(root, 1);
#endif
#ifdef HAVE_RECOVERY
    case A_RECOVERY: return rc_open(root, 1);
#endif
#ifdef HAVE_DOOM
    case A_DOOM: return dm_open(root, 1);
#endif
#if defined(NET_APP) && defined(HAVE_WEB)
    case A_NET: return net_open(root, 1);
#ifndef NET_DIRECT_ONLY
    case A_AI: return ai_open(root, 1);
    case A_DIAL: return dial_open(root, 1);
#endif
#endif
#if defined(HAVE_WEB) && !defined(NO_PCBRIDGE)
    case A_WEB: return br_open(root, 1);
    case A_TEXT: return wr_open(root, 1);
#endif
    }
    return 0;
}

static void mt_rect(int i, int *x, int *y, int *w, int *h) {
    int pos = i % MT_PER; *x = 10 + (pos % 2) * 120; *y = 50 + (pos / 2) * 102; *w = 116; *h = 94;
}
static void micon(u16 *g, int i, int cx, int cy, uint16_t fg, uint16_t bg) {
    switch (i) {
    case I_DIAL:                                                             /* Dialer: a phone handset */
        rrect(g, cx - 22, cy - 6, 44, 14, 6, fg); rrect(g, cx - 24, cy - 6, 14, 26, 6, fg); rrect(g, cx + 10, cy - 6, 14, 26, 6, fg); rrect(g, cx - 12, cy - 22, 24, 14, 5, fg); rect(g, cx - 6, cy - 4, 12, 6, bg); break;
    case I_RECOVERY:                                                         /* Recovery: a life ring with a cross */
        disc(g, cx, cy, 22, fg); disc(g, cx, cy, 14, bg); rect(g, cx - 3, cy - 10, 6, 20, fg); rect(g, cx - 10, cy - 3, 20, 6, fg); break;
    case I_APPS:                                                             /* My apps: four tiles and a plus */
        rrect(g, cx - 22, cy - 22, 20, 20, 5, fg); rrect(g, cx + 2, cy - 22, 20, 20, 5, fg); rrect(g, cx - 22, cy + 2, 20, 20, 5, fg); rrect(g, cx + 2, cy + 2, 20, 20, 5, bg); rect(g, cx + 10, cy + 6, 4, 12, fg); rect(g, cx + 6, cy + 10, 12, 4, fg); break;
    case I_GAMES2:                                                           /* More games: a small gamepad */
        rrect(g, cx - 24, cy - 12, 48, 28, 12, fg); rect(g, cx - 16, cy - 2, 12, 4, bg); rect(g, cx - 12, cy - 6, 4, 12, bg); disc(g, cx + 10, cy - 2, 3, bg); disc(g, cx + 17, cy + 5, 3, bg); break;
    case I_TOOLS:                                                            /* Tools: a wrench-like gear */
        disc(g, cx, cy, 17, fg); disc(g, cx, cy, 8, bg); for (int k = 0; k < 4; k++) { int dx = (k & 1) ? 17 : -17, dy = (k & 2) ? 17 : -17; (void)dy; } rect(g, cx - 4, cy - 25, 8, 12, fg); rect(g, cx - 4, cy + 13, 8, 12, fg); rect(g, cx - 25, cy - 4, 12, 8, fg); rect(g, cx + 13, cy - 4, 12, 8, fg); break;
    case I_GB:                                                               /* Game Boy: a handheld */
        rrect(g, cx - 17, cy - 24, 34, 48, 5, fg); rect(g, cx - 12, cy - 19, 24, 20, bg); rect(g, cx - 10, cy - 17, 20, 16, fg);
        rect(g, cx - 12, cy + 7, 10, 3, bg); rect(g, cx - 9, cy + 4, 4, 9, bg); disc(g, cx + 8, cy + 6, 3, bg); disc(g, cx + 3, cy + 12, 3, bg); break;
    case I_AI:                                                               /* AI: a sparkle */
        rrect(g, cx - 4, cy - 24, 8, 48, 4, fg); rrect(g, cx - 24, cy - 4, 48, 8, 4, fg); disc(g, cx - 14, cy - 14, 5, fg); disc(g, cx + 14, cy - 14, 5, fg); disc(g, cx - 14, cy + 14, 5, fg); disc(g, cx + 14, cy + 14, 5, fg); disc(g, cx, cy, 9, bg); disc(g, cx, cy, 5, fg); break;
    case I_NET:                                                              /* Internet: signal bars */
        rrect(g, cx - 22, cy + 6, 9, 14, 3, fg); rrect(g, cx - 10, cy - 2, 9, 22, 3, fg); rrect(g, cx + 2, cy - 12, 9, 32, 3, fg); rrect(g, cx + 14, cy - 22, 9, 42, 3, fg); break;
    case 0:                                                                  /* Snake: an S of squares */
        rrect(g, cx + 3, cy - 19, 10, 10, 3, fg); rrect(g, cx - 9, cy - 19, 10, 10, 3, fg); rrect(g, cx - 9, cy - 7, 10, 10, 3, fg);
        rrect(g, cx + 3, cy - 7, 10, 10, 3, fg); rrect(g, cx + 3, cy + 5, 10, 10, 3, fg); rrect(g, cx - 9, cy + 5, 10, 10, 3, fg);
        disc(g, cx - 4, cy + 10, 2, bg); break;
    case 1:                                                                  /* Flappy: a bird */
        disc(g, cx - 2, cy, 17, fg); disc(g, cx + 6, cy - 6, 5, bg); disc(g, cx + 7, cy - 6, 2, fg);
        rrect(g, cx + 12, cy - 1, 14, 9, 4, fg); rrect(g, cx - 16, cy + 1, 18, 10, 5, bg); break;
    case 2:                                                                  /* Tetris: T + block */
        rrect(g, cx - 17, cy - 17, 10, 10, 3, fg); rrect(g, cx - 5, cy - 17, 10, 10, 3, fg); rrect(g, cx + 7, cy - 17, 10, 10, 3, fg);
        rrect(g, cx - 5, cy - 5, 10, 10, 3, fg); rrect(g, cx - 17, cy + 7, 10, 10, 3, fg); rrect(g, cx - 5, cy + 7, 10, 10, 3, fg); break;
    case 3: rrect(g, cx - 22, cy - 20, 44, 40, 10, fg); ctext(g, cx, cy - 18, "2", 3, bg); break;   /* 2048 */
    case 4:                                                                  /* FPS / 3D: crosshair */
        disc(g, cx, cy, 20, fg); disc(g, cx, cy, 14, bg); disc(g, cx, cy, 3, fg);
        rect(g, cx - 2, cy - 27, 4, 11, fg); rect(g, cx - 2, cy + 16, 4, 11, fg); rect(g, cx - 27, cy - 2, 11, 4, fg); rect(g, cx + 16, cy - 2, 11, 4, fg); break;
    case 5:                                                                  /* Web: globe */
        disc(g, cx, cy, 21, fg); disc(g, cx, cy, 17, bg); rrect(g, cx - 9, cy - 17, 18, 34, 9, fg); rrect(g, cx - 6, cy - 17, 12, 34, 6, bg);
        rect(g, cx - 17, cy - 2, 34, 4, fg); break;
    case 6: rrect(g, cx - 20, cy - 17, 40, 8, 4, fg); rrect(g, cx - 20, cy - 4, 40, 8, 4, fg); rrect(g, cx - 20, cy + 9, 26, 8, 4, fg); break;   /* Texto */
    case 7: ctext(g, cx, cy - 24, "?", 4, fg); break;                        /* Ajuda */
    case 8:                                                                  /* Doom: skull */
        disc(g, cx, cy - 4, 19, fg); rrect(g, cx - 12, cy + 6, 24, 16, 5, fg);
        disc(g, cx - 8, cy - 5, 6, bg); disc(g, cx + 8, cy - 5, 6, bg); rect(g, cx - 2, cy + 3, 4, 6, bg);
        rect(g, cx - 7, cy + 14, 3, 8, bg); rect(g, cx - 1, cy + 14, 3, 8, bg); rect(g, cx + 5, cy + 14, 3, 8, bg); break;
    default: ctext(g, cx, cy - 24, "X", 4, fg); break;                       /* Fechar */
    }
}

static void menu_draw(MSt *m) {
    u16 *g = m->px; rect(g, 0, 0, KBD_W, KBD_H, P_BG);
    if (m->mode == 1) {
        ptext(g, 14, 12, TR("Ajuda", "Help"), 3, P_TEXT);
#ifndef HAVE_MINIGAMES
#define HELP_GAMES_PT ""
#define HELP_GAMES_EN ""
#else
#define HELP_GAMES_PT "Snake: toque na metade esquerda ou direita para virar.\nFlappy: toque para voar.\nTetris: botoes < R > v.\n2048: deslize para mover.\n\n"
#define HELP_GAMES_EN "Snake: tap the left or right half to turn.\nFlappy: tap to fly.\nTetris: buttons < R > v.\n2048: swipe to move.\n\n"
#endif
        draw_wrapped(g, TR(
            "Teclado T9: toque nas teclas como em celular antigo. Toque de novo na mesma tecla para trocar a letra; espere um instante para a pr" "\xc3\xb3" "xima. "
            "A tecla ' coloca acento na pr" "\xc3\xb3" "xima letra, Abc muda mai" "\xc3\xba" "sculas.\n\n"
            "Responda uma notifica" "\xc3\xa7" "\xc3\xa3" "o com a resposta r" "\xc3\xa1" "pida ... (tr" "\xc3\xaa" "s pontos).\n\n" HELP_GAMES_PT "Toque para voltar.",
            "T9 keyboard: tap the keys like on an old phone. Tap the same key again to change the letter; wait a moment for the next one. "
            "The ' key puts an accent on the next letter, Abc changes the case.\n\n"
            "Reply to a notification with the quick reply ... (three dots).\n\n" HELP_GAMES_EN "Tap to go back."),
            12, 62, 1, 29, 27, 0, P_TEXT, 0);
        return;
    }
    ptext(g, 14, 8, "Apps", 3, P_TEXT);
    { char pg[4]; pg[0] = (char)('1' + m->page); pg[1] = '/'; pg[2] = (char)('0' + MT_PAGES); pg[3] = 0; ptext(g, KBD_W - 14 - 48, 16, pg, 2, P_MUTED); }
    for (int j = 0; j < MT_PER; j++) {
        int i = m->page * MT_PER + j; if (i >= MT_N) break;
        int x, y, w, h; mt_rect(i, &x, &y, &w, &h);
        card(g, x, y, w, h, 28, mtile[i].bg);
        micon(g, mtile[i].icon, x + w / 2, y + 36, mtile[i].fg, mtile[i].bg);
        { const char *lb = TR(mtile[i].pt, mtile[i].en); int ln = 0; while (lb[ln]) ln++; if (ln > 7) ctext(g, x + w / 2, y + h - 22, lb, 1, mtile[i].fg); else ctext(g, x + w / 2, y + h - 28, lb, 2, mtile[i].fg); }
    }
    if (MT_PAGES > 1) { uint16_t on = m->page > 0 ? P_CARD : C(236, 230, 244), on2 = m->page < MT_PAGES - 1 ? P_CARD : C(236, 230, 244);
      card(g, 10, 358, 76, 34, 17, on); ctext(g, 48, 361, "<", 3, m->page > 0 ? P_ACCENT : P_MUTED);
      card(g, 170, 358, 76, 34, 17, on2); ctext(g, 208, 361, ">", 3, m->page < MT_PAGES - 1 ? P_ACCENT : P_MUTED);
      for (int p = 0; p < MT_PAGES; p++) disc(g, 128 - (MT_PAGES - 1) * 9 + p * 18, 375, p == m->page ? 6 : 4, p == m->page ? P_ACCENT : C(206, 196, 224)); }
}
static void menu_page(MSt *m, int np) { if (np < 0 || np >= MT_PAGES || np == m->page) return; m->page = np; menu_draw(m); INVALIDATE(m->img); }
static void menu_event(void *e) {
    int code = EV_CODE(e), x, y; MSt *m = (MSt *)EV_USER(e);
    if (code == 1 || code == 2) {                                               /* press / pressing: remember where it started */
        if (touch_read(&x, &y)) { if (!m->down) { m->down = 1; m->sx = x; m->sy = y; m->swiped = 0; } m->x = x; m->y = y; }
        return;
    }
    if (code == 8 || code == 3) {                                               /* released: horizontal swipe changes the page */
        if (m->mode == 0 && m->down) {
            int dx = m->x - m->sx, dy = m->y - m->sy;
            if ((dx > 50 || dx < -50) && dx * dx > 4 * dy * dy) { m->swiped = 1; menu_page(m, m->page + (dx < 0 ? 1 : -1)); }
        }
        m->down = 0; return;
    }
    if (code != 7) return;                                                      /* a tap */
    if (m->swiped) { m->swiped = 0; return; }
    if (m->mode == 1) { m->mode = 0; menu_draw(m); INVALIDATE(m->img); return; }
    if (MT_PAGES > 1 && m->y >= 356) { if (m->x < 100) menu_page(m, m->page - 1); else if (m->x >= 156) menu_page(m, m->page + 1); return; }
    for (int j = 0; j < MT_PER; j++) {
        int i = m->page * MT_PER + j, tx, ty, tw, th; if (i >= MT_N) break;
        mt_rect(i, &tx, &ty, &tw, &th);
        if (m->x < tx || m->x >= tx + tw || m->y < ty || m->y >= ty + th) continue;
        if (mtile[i].act == A_HELP) { m->mode = 1; menu_draw(m); INVALIDATE(m->img); return; }
        ADD_FLAG(m->img, 1); FREE(m->cvblk);                                   /* hide + give the 205 KB back */
        if (mtile[i].act == A_CLOSE) return;
        if (!menu_launch(m->root, mtile[i].act)) { MOTOR_ONCE(5, 0); menu_open(m->root); }
        return;
    }
}
static int menu_open(void *root) {
    MSt *m = (MSt *)MALLOC(sizeof(MSt)); if (!m) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!blk) { FREE(m); return 0; }
    m->cvblk = blk; m->px = (u16 *)((u8 *)blk + 8); m->root = root; m->mode = 0; m->page = 0; m->x = m->y = m->sx = m->sy = 0; m->down = m->swiped = 0;
    menu_draw(m);
    m->img = make_canvas(root, m->dsc, m->px, (void *)menu_event, m);
    return 1;
}
static void menu_timer(void *timer) {                                   /* one-shot: open the menu after the page finished building */
    void *root = *(void **)((u8 *)timer + 0xC);
    TIMER_DEL(timer);
    menu_open(root);
}
/* hook: app_settings_tutorials_goto_sub_page(page): called with r4 = page root, r6 = page. Pages 1/2 are the tutorial sub-pages. */
int menu_hook(void *root, int page) {
    if (!root || page == 1 || page == 2) return 0;
    TIMER_CREATE((void *)menu_timer, 80, root);
    return 0;
}
