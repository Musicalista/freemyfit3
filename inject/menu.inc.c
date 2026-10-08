/* ================= Settings menu ("Apps extras") =================
 * Opened from Settings > (tutorials entry, relabelled "Apps extras") by a one-shot LVGL timer so it lands ON TOP of the page's own widgets.
 * 2x3 grid: Snake, Flappy, Tetris, 3D game, Help, Close. Included by fit3_apps.c after the games (uses their helpers). */
typedef struct { u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img; int x, y, mode; } MSt;
#define MT_N 9
static const struct { int x, y, w, h; const char *label; uint16_t col; } mtile[MT_N] = {
    { 6, 40, 118, 54, "Snake", C(30, 120, 60) }, { 132, 40, 118, 54, "Flappy", C(40, 110, 170) },
    { 6, 100, 118, 54, "Tetris", C(110, 50, 150) }, { 132, 100, 118, 54, "2048", C(200, 140, 60) },
    { 6, 160, 118, 54, GAME_LABEL, C(150, 70, 40) }, { 132, 160, 118, 54, "Web", C(20, 130, 130) },
    { 6, 220, 118, 54, "Texto", C(40, 100, 100) }, { 132, 220, 118, 54, "Ajuda", C(50, 70, 120) },
    { 6, 280, 244, 54, "Fechar", C(120, 40, 40) } };

static void menu_draw(MSt *m) {
    u16 *g = m->px; rect(g, 0, 0, KBD_W, KBD_H, C(16, 16, 22));
    if (m->mode == 1) {
        draw_wrapped(g,
            "Ajuda\n\n"
            "Teclado: responda uma notifica" "\xc3\xa7" "\xc3\xa3" "o com a resposta r" "\xc3\xa1" "pida ... (tr" "\xc3\xaa" "s pontos). "
            "A mensagem aparece no topo; digite e toque OK.\n\n"
            "Snake: toque na metade esquerda ou direita para virar.\n"
            "Flappy: toque para voar.\n"
            "Tetris: botoes < R > v (R gira, v desce).\n"
            "2048: deslize para mover (ou use as setas).\n"
            "3D: tambem pela resposta rapida 3d.\n\n"
            "X no canto fecha. Toque para voltar.",
            6, 10, 1, 31, 28, 0, C(235, 235, 240), 0);
        return;
    }
    ptext(g, 24, 14, "APPS EXTRAS", 2, C(255, 210, 90));
    for (int i = 0; i < MT_N; i++) {
        int len = 0; while (mtile[i].label[len]) len++;
        rect(g, mtile[i].x, mtile[i].y, mtile[i].w, mtile[i].h, mtile[i].col);
        ptext(g, mtile[i].x + mtile[i].w / 2 - len * 8, mtile[i].y + 20, mtile[i].label, 2, C(255, 255, 255));
    }
    draw_wrapped(g, "mod AZA3 - Fit3", 6, 380, 1, 31, 1, 0, C(110, 120, 150), 0);
}
static void menu_event(void *e) {
    int code = EV_CODE(e), x, y; MSt *m = (MSt *)EV_USER(e);
    if (code == 1 || code == 2) { if (touch_read(&x, &y)) { m->x = x; m->y = y; } return; }
    if (code != 7) return;                                              /* a tap */
    if (m->mode == 1) { m->mode = 0; menu_draw(m); INVALIDATE(m->img); return; }
    for (int i = 0; i < MT_N; i++) {
        if (m->x < mtile[i].x || m->x >= mtile[i].x + mtile[i].w || m->y < mtile[i].y || m->y >= mtile[i].y + mtile[i].h) continue;
        if (i == 7) { m->mode = 1; menu_draw(m); INVALIDATE(m->img); return; }
        ADD_FLAG(m->img, 1); FREE(m->cvblk);                           /* hide + give the 205 KB back */
        if (i == 8) return;
        int ok = (i < 4) ? mg_open(m->root, i, 1) : (i == 4) ? game_open(m->root, 1) : (i == 5) ? br_open(m->root, 1) : wr_open(m->root, 1);
        if (!ok) { MOTOR_ONCE(5, 0); menu_open(m->root); }
        return;
    }
}
static int menu_open(void *root) {
    MSt *m = (MSt *)MALLOC(sizeof(MSt)); if (!m) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!blk) { FREE(m); return 0; }
    m->cvblk = blk; m->px = (u16 *)((u8 *)blk + 8); m->root = root; m->mode = 0; m->x = m->y = 0;
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
