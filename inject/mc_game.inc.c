/* ================= Block world game: title menu, world generation, creative + survival (mobs), hotbar + inventory, save (glue for src/vox.c) =================
 * Screens: title menu (continue / new world / world type / mode / seed / exit), play, pause menu, inventory, death.
 * Play: drag on the view = look around, tap = hit a mob or break a block, long press = place, hotbar slot tap = select, arrows move/strafe, U/D jump or fly up/down.
 * Survival: hearts + hunger, drops go to the inventory, crafting, zombies at night, pigs (pork) by day. Creative: flight and unlimited blocks.
 * One finger only (touch panel). The world is saved to /user/mc.sav (run-length coded) on request, every ~2 minutes and on exit. */
#define RW 128
#define RH 131                 /* internal render size (blitted 2x into the top 262 rows of the 256x402 canvas) */
#define GAME_LABEL "3D"
#ifndef FS_TABLE
#define FS_TABLE ((u32 *)0x200f73b8)
#endif
#ifndef FS_OPEN
#define FS_OPEN ((int (*)(const char *, const char *))FS_TABLE[0])
#endif
#ifndef FS_READ
#define FS_READ ((int (*)(int, void *, u32))FS_TABLE[1])
#endif
#ifndef FS_WRITE
#define FS_WRITE ((int (*)(int, const void *, u32))FS_TABLE[2])
#endif
#ifndef FS_CLOSE
#define FS_CLOSE ((int (*)(int))FS_TABLE[3])
#endif
#define MC_SAVE "/user/mc.sav"
#define MC_VIEW_H 262
enum { MS_MENU, MS_PLAY, MS_INV, MS_PAUSE, MS_DEAD };
typedef struct {
    u32 dsc[4]; u16 *cv; u16 *rfb, *zb, *texpix; Vox *g; void *img, *timer, *root; u32 *cvblk; int from_menu;
    int screen, dirty, pressed, x, y, moved, sx, sy, lx, ly, v0, fps, frames, wtype, survival, has_save, msg, save_t, prev_hurt; u32 fps_t, seed; M3dTex tex;
} GSt;
typedef struct { int x, y, w, h; char label; } Btn;
enum { B_FWD, B_LEFT, B_BACK, B_RIGHT, B_UP, B_DN, B_FLY, B_INV, B_EAT, B_N };
static const Btn btns[B_N] = {
    {56, 266, 52, 38, '^'}, {4, 308, 52, 40, '<'}, {56, 308, 52, 40, 'v'}, {108, 308, 52, 40, '>'},
    {164, 308, 44, 40, 'U'}, {210, 308, 42, 40, 'D'}, {164, 266, 44, 38, 0}, {210, 266, 42, 38, 0}, {4, 266, 48, 38, 0} };
static int hit_btn(int x, int y) {
    for (int i = 0; i < B_N; i++) if (x >= btns[i].x && x < btns[i].x + btns[i].w && y >= btns[i].y && y < btns[i].y + btns[i].h) return i;
    return -1;
}
static int hot_slot(int x, int y) { if (y < 352 || y >= 402 || x < 2 || x >= 254) return -1; int i = (x - 2) / 28; return i < 9 ? i : -1; }
static int menu_btn_hit(int x, int y) { return x >= 214 && y < 30; }
static const char *const bl_pt[VB_N] = { "", "Grama", "Terra", "Pedra", "Pedregulho", "Areia", "Tronco", "Tabuas", "Folhas", "Tijolo", "Vidro", "La vermelha", "La azul", "Neve", "Carvao", "Rocha base" };
static const char *const bl_en[VB_N] = { "", "Grass", "Dirt", "Stone", "Cobblestone", "Sand", "Log", "Planks", "Leaves", "Brick", "Glass", "Red wool", "Blue wool", "Snow", "Coal ore", "Bedrock" };
static const char *wt_name(int t) {
    static const char *const pt[VW_TYPES] = { "Colinas", "Plano", "Montanhas", "Deserto" }, *const en[VW_TYPES] = { "Hills", "Flat", "Mountains", "Desert" };
    return TR(pt[t], en[t]);
}

static int menu_open(void *root);
static void mc_icon(GSt *st, int x, int y, int sz, int id) {                       /* a block's side texture, scaled to sz px */
    u16 *cv = st->cv; int t = vx_face_tile(id, 1) * 16;
    for (int dy = 0; dy < sz; dy++) { const u16 *s = st->tex.pix + (dy * 16 / sz) * VX_TEXW + t; u16 *d = cv + (y + dy) * KBD_W + x; for (int dx = 0; dx < sz; dx++) d[dx] = s[dx * 16 / sz]; }
}
static void mc_btn(u16 *g, int x, int y, int w, int h, const char *s, int sc, u16 bg, u16 fg) { rrect(g, x, y, w, h, 10, bg); ctext(g, x + w / 2, y + (h - 12 * sc) / 2, s, sc, fg); }
static void mc_hotbar(GSt *st) {
    u16 *cv = st->cv; Vox *g = st->g; rect(cv, 0, 350, KBD_W, 52, C(16, 18, 26));
    for (int i = 0; i < 9; i++) {
        int x = 2 + i * 28, sel = i == g->sel, cnt = g->inv[g->hot[i]];
        rect(cv, x, 354, 26, 46, sel ? C(240, 200, 60) : C(48, 52, 70)); rect(cv, x + 2, 356, 22, 42, C(28, 30, 42));
        mc_icon(st, x + 3, 358, 20, g->hot[i]);
        if (g->survival) { if (cnt) mg_num(cv, x + 4, 382, cnt > 99 ? 99 : cnt, 1, C(255, 255, 255)); else for (int k = 0; k < 20; k++) for (int j = 0; j < 20; j++) if ((k + j) & 1) cv[(358 + j) * KBD_W + x + 3 + k] = C(28, 30, 42); }
    }
}
static void mc_controls(GSt *st) {
    u16 *cv = st->cv; int hb = st->pressed ? hit_btn(st->x, st->y) : -1; Vox *g = st->g;
    rect(cv, 0, MC_VIEW_H, KBD_W, 90, C(16, 18, 26));
    for (int i = 0; i < B_N; i++) {
        const Btn *b = &btns[i]; if ((i == B_EAT || i == B_FLY) && (i == B_EAT) != g->survival) continue;
        int on = (i == hb) || (i == B_FLY && g->flying);
        mc_btn(cv, b->x, b->y, b->w, b->h, "", 1, on ? C(220, 160, 30) : C(44, 48, 66), C(255, 255, 255));
        if (b->label) put(cv, b->x + b->w / 2 - 8, b->y + b->h / 2 - 12, (uint32_t)(uint8_t)b->label, 2, C(255, 255, 255));
    }
    if (g->survival) { ctext(cv, btns[B_EAT].x + btns[B_EAT].w / 2, btns[B_EAT].y + 5, TR("COMER", "EAT"), 1, C(255, 255, 255)); mg_num(cv, btns[B_EAT].x + 18, btns[B_EAT].y + 20, g->inv[VB_PORK], 1, C(250, 200, 120)); }
    else ctext(cv, btns[B_FLY].x + btns[B_FLY].w / 2, btns[B_FLY].y + 13, TR("VOAR", "FLY"), 1, C(255, 255, 255));
    ctext(cv, btns[B_INV].x + btns[B_INV].w / 2, btns[B_INV].y + 13, TR("ITENS", "ITEMS"), 1, C(255, 255, 255));
    mc_hotbar(st);
}
static void mc_hud(GSt *st) {
    u16 *cv = st->cv; Vox *g = st->g; rect(cv, 0, 0, 40, 28, C(0, 0, 0)); put(cv, 2, 2, (uint32_t)('0' + st->fps / 10 % 10), 2, C(255, 255, 0)); put(cv, 18, 2, (uint32_t)('0' + st->fps % 10), 2, C(255, 255, 0));
    rect(cv, 216, 0, 40, 28, C(40, 44, 60)); ctext(cv, 236, 6, "II", 2, C(255, 255, 255));
    if (!g->survival) { if (g->flying) ctext(cv, 128, 4, TR("VOANDO", "FLYING"), 1, C(255, 255, 255)); return; }
    for (int i = 0; i < 10; i++) {                                                       /* hearts (left) and drumsticks (right), 2 points each */
        int hv = g->health - i * 2, fv = g->food - i * 2; int hx = 44 + i * 11, fx = 44 + i * 11;
        rect(cv, hx, 2, 9, 8, C(40, 10, 10)); if (hv >= 2) rect(cv, hx + 1, 3, 7, 6, C(230, 40, 50)); else if (hv == 1) rect(cv, hx + 1, 3, 3, 6, C(230, 40, 50));
        rect(cv, fx, 13, 9, 8, C(50, 30, 10)); if (fv >= 2) rect(cv, fx + 1, 14, 7, 6, C(220, 150, 50)); else if (fv == 1) rect(cv, fx + 1, 14, 3, 6, C(220, 150, 50));
    }
    { uint32_t p = g->time % VX_DAY_TICKS; int night = p >= (uint32_t)(VX_DAY_TICKS * 5 / 10) && p < (uint32_t)(VX_DAY_TICKS * 88 / 100); ctext(cv, 128, 40, night ? TR("NOITE", "NIGHT") : TR("DIA", "DAY"), 1, night ? C(150, 170, 255) : C(255, 240, 150)); }
    if (g->hurt > 0) { int t = g->hurt > 8 ? 4 : 2; rect(cv, 0, 0, KBD_W, t, C(220, 20, 20)); rect(cv, 0, MC_VIEW_H - t, KBD_W, t, C(220, 20, 20)); rect(cv, 0, 0, t, MC_VIEW_H, C(220, 20, 20)); rect(cv, KBD_W - t, 0, t, MC_VIEW_H, C(220, 20, 20)); }
}

/* ---------------- save / load (run-length coded blocks) ---------------- */
static void mc_put32(u8 *b, u32 v) { b[0] = (u8)v; b[1] = (u8)(v >> 8); b[2] = (u8)(v >> 16); b[3] = (u8)(v >> 24); }
static u32 mc_get32(const u8 *b) { return (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) | ((u32)b[3] << 24); }
static void mc_putf(u8 *b, float f) { union { float f; u32 u; } c; c.f = f; mc_put32(b, c.u); }
static float mc_getf(const u8 *b) { union { float f; u32 u; } c; c.u = mc_get32(b); return c.f; }
#define MC_HDR 64
static int mc_save(GSt *st) {
    Vox *g = st->g; int fd = FS_OPEN(MC_SAVE, "w"); if (fd < 0) return 0;
    u8 *buf = (u8 *)MALLOC(4096); if (!buf) { FS_CLOSE(fd); return 0; }
    for (int i = 0; i < MC_HDR; i++) buf[i] = 0;
    buf[0] = 'V'; buf[1] = 'X'; buf[2] = '2'; mc_put32(buf + 4, g->seed); mc_putf(buf + 8, g->x); mc_putf(buf + 12, g->y); mc_putf(buf + 16, g->z); mc_putf(buf + 20, g->yaw); mc_putf(buf + 24, g->pitch);
    buf[28] = (u8)g->flying; buf[29] = (u8)g->sel; for (int i = 0; i < 9; i++) buf[30 + i] = g->hot[i]; buf[39] = (u8)g->wtype;
    buf[40] = (u8)g->survival; buf[41] = (u8)g->health; buf[42] = (u8)g->food; mc_put32(buf + 43, g->time); for (int i = 0; i <= VB_N; i++) buf[47 + i] = g->inv[i];
    int n = MC_HDR, ok = 1; const int N = VX_W * VX_H * VX_D;
    for (int i = 0; i < N;) {
        u8 v = g->blk[i]; int run = 1; while (i + run < N && g->blk[i + run] == v && run < 255) run++;
        if (n + 2 > 4096) { if (FS_WRITE(fd, buf, (u32)n) < 0) ok = 0; n = 0; }
        buf[n++] = (u8)run; buf[n++] = v; i += run;
    }
    if (n && FS_WRITE(fd, buf, (u32)n) < 0) ok = 0;
    FS_CLOSE(fd); FREE(buf); if (ok) g->modified = 0; return ok;
}
static int mc_load(GSt *st) {
    Vox *g = st->g; int fd = FS_OPEN(MC_SAVE, "r"); if (fd < 0) return 0;
    u8 *buf = (u8 *)MALLOC(4100); if (!buf) { FS_CLOSE(fd); return 0; }
    int n = 0, r; while (n < MC_HDR && (r = FS_READ(fd, buf + n, (u32)(MC_HDR - n))) > 0) n += r;
    if (n < MC_HDR || buf[0] != 'V' || buf[1] != 'X' || buf[2] != '2') { FS_CLOSE(fd); FREE(buf); return 0; }
    g->seed = mc_get32(buf + 4); g->x = mc_getf(buf + 8); g->y = mc_getf(buf + 12); g->z = mc_getf(buf + 16); g->yaw = mc_getf(buf + 20); g->pitch = mc_getf(buf + 24);
    g->flying = buf[28] & 1; g->sel = buf[29] < 9 ? buf[29] : 0; for (int i = 0; i < 9; i++) g->hot[i] = buf[30 + i] < VB_N ? buf[30 + i] : VB_STONE; g->wtype = buf[39] < VW_TYPES ? buf[39] : 0;
    g->survival = buf[40] & 1; g->health = buf[41] > 20 ? 20 : buf[41]; g->food = buf[42] > 20 ? 20 : buf[42]; g->time = mc_get32(buf + 43); for (int i = 0; i <= VB_N; i++) g->inv[i] = buf[47 + i];
    if (g->survival && g->health < 1) g->health = 20;
    const int N = VX_W * VX_H * VX_D; int pos = 0, carry = 0;
    while (pos < N && (r = FS_READ(fd, buf + carry, 4096)) > 0) {
        int len = carry + r, i = 0;
        while (i + 1 < len && pos < N) { int run = buf[i], v = buf[i + 1]; for (int k = 0; k < run && pos < N; k++) g->blk[pos++] = (u8)(v < VB_N ? v : 0); i += 2; }
        carry = len - i; for (int k = 0; k < carry; k++) buf[k] = buf[i + k];
    }
    FS_CLOSE(fd); FREE(buf);
    if (pos < N) return 0;
    vx_recalc_tops(g); g->radius = 11; g->has_hit = 0; g->xo = g->x; g->yo = g->y; g->zo = g->z; g->xd = g->yd = g->zd = 0; g->modified = 0; g->turn_yaw = g->turn_pitch = 0;
    g->dead = 0; g->hurt = 0; g->hungert = g->regent = g->spawnt = 0; g->rng = g->seed * 2654435761u + g->time + 1u;
    for (int i = 0; i < VK_N; i++) g->held[i] = 0;
    for (int i = 0; i < VX_MOBS; i++) g->mobs[i].type = VM_NONE;
    return 1;
}
static int mc_probe(void) {
    int fd = FS_OPEN(MC_SAVE, "r"); if (fd < 0) return 0; u8 b[4]; int n = 0, r;
    while (n < 4 && (r = FS_READ(fd, b + n, (u32)(4 - n))) > 0) n += r; FS_CLOSE(fd); return n == 4 && b[0] == 'V' && b[1] == 'X' && b[2] == '2';
}

/* ---------------- screens ---------------- */
#define MN_Y0 108
#define MN_H 38
#define MN_STEP 43
static void mc_menu_draw(GSt *st) {
    u16 *cv = st->cv;
    for (int y = 0; y < KBD_H; y += 2) { int k = y * 60 / KBD_H; rect(cv, 0, y, KBD_W, 2, C(70 + k, 140 + k, 220)); }
    ctext(cv, 128, 18, "BLOCKS 3D", 3, C(255, 255, 255)); ctext(cv, 128, 56, TR("mundo de blocos", "block world"), 1, C(230, 240, 255));
    for (int i = 0; i < VB_PLACEABLE; i++) mc_icon(st, 9 + i * 17, 80, 16, i + 1);
    mc_btn(cv, 12, MN_Y0, 232, MN_H, TR("Continuar", "Continue"), 2, st->has_save ? C(110, 200, 120) : C(130, 150, 170), st->has_save ? C(10, 50, 20) : C(220, 230, 240));
    mc_btn(cv, 12, MN_Y0 + MN_STEP, 232, MN_H, TR("Novo mundo", "New world"), 2, C(250, 214, 90), C(70, 50, 0));
    { char tb[40]; int n = 0; const char *p = TR("Tipo: ", "Type: "); while (*p) tb[n++] = *p++; p = wt_name(st->wtype); while (*p) tb[n++] = *p++; tb[n] = 0; mc_btn(cv, 12, MN_Y0 + MN_STEP * 2, 232, MN_H, tb, 2, C(235, 240, 250), C(30, 50, 100)); }
    { char tb[40]; int n = 0; const char *p = TR("Modo: ", "Mode: "); while (*p) tb[n++] = *p++; p = st->survival ? TR("Sobreviv.", "Survival") : TR("Criativo", "Creative"); while (*p) tb[n++] = *p++; tb[n] = 0; mc_btn(cv, 12, MN_Y0 + MN_STEP * 3, 232, MN_H, tb, 2, st->survival ? C(250, 190, 190) : C(235, 240, 250), C(30, 50, 100)); }
    { char tb[40]; int n = 0; const char *p = TR("Semente: ", "Seed: "); while (*p) tb[n++] = *p++; char d[12]; int k = 0; u32 v = st->seed % 100000u; do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v); while (k) tb[n++] = d[--k]; tb[n] = 0;
      mc_btn(cv, 12, MN_Y0 + MN_STEP * 4, 232, MN_H, tb, 2, C(235, 240, 250), C(30, 50, 100)); }
    mc_btn(cv, 12, MN_Y0 + MN_STEP * 5, 232, MN_H, TR("Sair", "Exit"), 2, C(220, 120, 120), C(60, 10, 10));
    if (st->msg) ctext(cv, 128, 376, st->msg == 1 ? TR("Nenhum mundo salvo", "No saved world") : st->msg == 2 ? TR("Falha ao carregar", "Load failed") : TR("Mundo salvo", "World saved"), 1, C(255, 255, 255));
}
static void mc_pause_draw(GSt *st) {
    u16 *cv = st->cv; rect(cv, 0, 0, KBD_W, MC_VIEW_H, C(20, 24, 36));
    ctext(cv, 128, 14, TR("Pausa", "Paused"), 3, C(255, 255, 255));
    mc_btn(cv, 12, 56, 232, 44, TR("Continuar", "Resume"), 2, C(110, 200, 120), C(10, 50, 20));
    mc_btn(cv, 12, 108, 232, 44, TR("Itens", "Items"), 2, C(250, 214, 90), C(70, 50, 0));
    mc_btn(cv, 12, 160, 232, 44, st->msg == 3 ? TR("Salvo!", "Saved!") : TR("Salvar mundo", "Save world"), 2, C(235, 240, 250), C(30, 50, 100));
    mc_btn(cv, 12, 212, 232, 44, TR("Menu principal", "Main menu"), 2, C(190, 200, 250), C(30, 40, 110));
}
static void mc_dead_draw(GSt *st) {
    u16 *cv = st->cv; rect(cv, 0, 0, KBD_W, MC_VIEW_H, C(70, 6, 6));
    ctext(cv, 128, 70, TR("Voce morreu", "You died"), 3, C(255, 220, 220));
    mc_btn(cv, 12, 140, 232, 44, TR("Renascer", "Respawn"), 2, C(110, 200, 120), C(10, 50, 20));
    mc_btn(cv, 12, 196, 232, 44, TR("Menu principal", "Main menu"), 2, C(190, 200, 250), C(30, 40, 110));
}
static void mc_inv_draw(GSt *st) {
    u16 *cv = st->cv; Vox *g = st->g; rect(cv, 0, 0, KBD_W, 350, C(24, 28, 40));
    ctext(cv, 128, 8, TR("Itens", "Items"), 3, C(255, 255, 255));
    { char tb[48]; int n = 0; const char *p = "Slot "; while (*p) tb[n++] = *p++; tb[n++] = (char)('1' + g->sel); tb[n++] = ':'; tb[n++] = ' '; p = TR(bl_pt[g->hot[g->sel]], bl_en[g->hot[g->sel]]); while (*p && n < 46) tb[n++] = *p++; tb[n] = 0; ctext(cv, 128, 42, tb, 1, C(250, 214, 90)); }
    for (int i = 0; i < VB_PLACEABLE; i++) {
        int cx = 8 + (i % 5) * 48, cy = 62 + (i / 5) * 64; int sel = g->hot[g->sel] == i + 1, none = g->survival && !g->inv[i + 1];
        rect(cv, cx, cy, 46, 62, sel ? C(240, 200, 60) : C(52, 58, 80)); rect(cv, cx + 2, cy + 2, 42, 58, C(32, 36, 52));
        mc_icon(st, cx + 7, cy + 5, 32, i + 1);
        if (none) for (int k = 0; k < 32; k++) for (int j = 0; j < 32; j++) if ((k + j) & 1) cv[(cy + 5 + j) * KBD_W + cx + 7 + k] = C(32, 36, 52);
        { static const char *const sp[VB_N] = { "", "Grama", "Terra", "Pedra", "Brita", "Areia", "Tora", "Tabua", "Folha", "Tijol", "Vidro", "La V.", "La A.", "Neve", "Carv." },
                                  *const se[VB_N] = { "", "Grass", "Dirt", "Stone", "Cobl.", "Sand", "Log", "Plank", "Leaf", "Brick", "Glass", "Red", "Blue", "Snow", "Coal" };
          if (g->survival) mg_num(cv, cx + 4, cy + 44, g->inv[i + 1], 1, C(255, 255, 255)); else ctext(cv, cx + 23, cy + 44, TR(sp[i + 1], se[i + 1]), 1, C(220, 226, 240)); }
    }
    if (g->survival) {                                                                  /* crafting: log -> 4 planks, 2 sand -> 2 glass, 4 cobble -> 2 brick */
        static const uint8_t from[3] = { VB_LOG, VB_SAND, VB_COBBLE }, to[3] = { VB_PLANKS, VB_GLASS, VB_BRICK }, nf[3] = { 1, 2, 4 }, nt[3] = { 4, 2, 2 };
        for (int k = 0; k < 3; k++) {
            int x = 8 + k * 78, ok = g->inv[from[k]] >= nf[k]; rect(cv, x, 258, 76, 38, ok ? C(80, 140, 90) : C(52, 58, 80));
            mc_icon(st, x + 3, 262, 16, from[k]); mg_num(cv, x + 21, 266, nf[k], 1, C(255, 255, 255)); ctext(cv, x + 38, 266, ">", 1, C(255, 255, 255)); mc_icon(st, x + 46, 262, 16, to[k]); mg_num(cv, x + 64, 266, nt[k], 1, C(255, 255, 255));
            ctext(cv, x + 38, 282, TR("fabricar", "craft"), 1, C(230, 236, 250));
        }
    }
    mc_btn(cv, 12, 302, 232, 40, TR("Voltar", "Back"), 2, C(110, 200, 120), C(10, 50, 20));
    mc_hotbar(st);
}

static void game_close(GSt *st) {
    TIMER_DEL(st->timer); ADD_FLAG(st->img, 1);
    if (st->screen != MS_MENU && st->g->modified) mc_save(st);
    FREE(st->cvblk); FREE(st->rfb); FREE(st->zb); FREE(st->texpix); FREE(st->g);
    if (st->from_menu) menu_open(st->root);                              /* back to the Apps extras menu */
}
static void mc_new(GSt *st) { u32 now = TICK_GET(); vx_gen(st->g, st->seed, st->wtype); vx_set_survival(st->g, st->survival); st->g->last_ms = (int64_t)now; st->screen = MS_PLAY; st->dirty = 1; st->msg = 0; st->prev_hurt = 0; }
static void mc_tap_menu(GSt *st, int x, int y) {
    if (x < 12 || x >= 244 || y < MN_Y0) return;
    int row = (y - MN_Y0) / MN_STEP; if (row > 5 || (y - MN_Y0) % MN_STEP >= MN_H) return;
    switch (row) {
    case 0:
        if (!st->has_save) { st->msg = 1; st->dirty = 1; return; }
        if (mc_load(st)) { st->g->last_ms = (int64_t)TICK_GET(); st->screen = MS_PLAY; st->msg = 0; st->survival = st->g->survival; st->prev_hurt = 0; } else st->msg = 2;
        st->dirty = 1; break;
    case 1: mc_new(st); break;
    case 2: st->wtype = (st->wtype + 1) % VW_TYPES; st->dirty = 1; break;
    case 3: st->survival ^= 1; st->dirty = 1; break;
    case 4: st->seed = st->seed * 1664525u + 1013904223u + TICK_GET(); st->dirty = 1; break;
    default: game_close(st); break;
    }
}
static void game_event(void *e) {
    int code = EV_CODE(e); GSt *st = (GSt *)EV_USER(e); int x, y;
    if (code == 1 || code == 2) {
        if (touch_read(&x, &y)) {
            if (code == 1 || !st->pressed) { st->sx = st->lx = x; st->sy = st->ly = y; st->moved = 0; st->v0 = y < MC_VIEW_H && !menu_btn_hit(x, y); }
            st->pressed = 1; st->x = x; st->y = y;
            if (x - st->sx > 12 || st->sx - x > 12 || y - st->sy > 12 || st->sy - y > 12) st->moved = 1;
        }
        return;
    }
    if (code == 8 || code == 3) { st->pressed = 0; return; }
    x = st->x; y = st->y;
    if (code == 7) {                                                      /* tap (a drag is not a tap: it looks around) */
        switch (st->screen) {
        case MS_MENU: mc_tap_menu(st, x, y); break;
        case MS_DEAD:
            if (x < 12 || x >= 244) break;
            if (y >= 140 && y < 184) { vx_respawn(st->g); st->g->last_ms = (int64_t)TICK_GET(); st->screen = MS_PLAY; st->prev_hurt = 0; }
            else if (y >= 196 && y < 240) { if (st->g->modified) mc_save(st); st->has_save = mc_probe(); st->screen = MS_MENU; st->msg = 0; }
            st->dirty = 1; break;
        case MS_PAUSE:
            if (x < 12 || x >= 244) break;
            if (y >= 56 && y < 100) { st->screen = MS_PLAY; st->g->last_ms = (int64_t)TICK_GET(); st->msg = 0; }
            else if (y >= 108 && y < 152) st->screen = MS_INV;
            else if (y >= 160 && y < 204) { st->msg = mc_save(st) ? 3 : 0; }
            else if (y >= 212 && y < 256) { if (st->g->modified) mc_save(st); st->has_save = mc_probe(); st->screen = MS_MENU; st->msg = 0; }
            st->dirty = 1; break;
        case MS_INV: {
            int s = hot_slot(x, y);
            if (s >= 0) st->g->sel = s;
            else if (y >= 302 && y < 342) { st->screen = MS_PAUSE; }
            else if (st->g->survival && y >= 258 && y < 296 && x >= 8 && x < 242) { if (vx_craft(st->g, (x - 8) / 78)) MOTOR_ONCE(1, 0); }
            else for (int i = 0; i < VB_PLACEABLE; i++) { int cx = 8 + (i % 5) * 48, cy = 62 + (i / 5) * 64; if (x >= cx && x < cx + 46 && y >= cy && y < cy + 62) { st->g->hot[st->g->sel] = (u8)(i + 1); break; } }
            st->dirty = 1; break; }
        default: {                                                        /* play */
            int b = hit_btn(x, y), s = hot_slot(x, y);
            if (menu_btn_hit(x, y)) { st->screen = MS_PAUSE; st->dirty = 1; st->msg = 0; }
            else if (b == B_FLY && !st->g->survival) vx_toggle_fly(st->g);
            else if (b == B_EAT && st->g->survival) { if (vx_eat(st->g)) MOTOR_ONCE(1, 0); }
            else if (b == B_INV) { st->screen = MS_INV; st->dirty = 1; }
            else if (s >= 0) st->g->sel = s;
            else if (y < MC_VIEW_H && !st->moved) { if (!vx_attack(st->g)) vx_break(st->g); }
            break; }
        }
    } else if (code == 5 && st->screen == MS_PLAY) {                      /* long press on the view: place the selected block */
        if (!st->moved && y < MC_VIEW_H && !menu_btn_hit(x, y)) vx_place(st->g);
    }
}
static void game_tick(void *timer) {
    GSt *st = *(GSt **)((u8 *)timer + 0xC); Vox *g = st->g; u32 now = TICK_GET();
    if (st->screen != MS_PLAY) {
        if (st->dirty) { st->dirty = 0; if (st->screen == MS_MENU) mc_menu_draw(st); else if (st->screen == MS_PAUSE) mc_pause_draw(st); else if (st->screen == MS_DEAD) mc_dead_draw(st); else mc_inv_draw(st); INVALIDATE(st->img); }
        return;
    }
    int hb = st->pressed ? hit_btn(st->x, st->y) : -1;
    if (st->pressed && st->v0 && st->moved) vx_look(g, (float)(st->x - st->lx) * 3.0f, (float)(st->ly - st->y) * 3.0f);   /* drag on the view = look around */
    st->lx = st->x; st->ly = st->y;
    vx_advance(g, (int64_t)now); int n = g->ticks; if (n > 3) n = 3;
    for (int i = 0; i < n; i++) {
        vx_key(g, VK_FWD, hb == B_FWD); vx_key(g, VK_BACK, hb == B_BACK); vx_key(g, VK_LEFT, hb == B_LEFT); vx_key(g, VK_RIGHT, hb == B_RIGHT);
        vx_key(g, VK_UP, hb == B_UP); vx_key(g, VK_DOWN, hb == B_DN);
        vx_tick(g);
    }
    if (g->hurt > st->prev_hurt) MOTOR_ONCE(1, 0);                       /* just took damage */
    st->prev_hurt = g->hurt;
    if (g->dead) { st->screen = MS_DEAD; st->dirty = 1; return; }
    vx_render(g, &st->tex, RW, RH, st->rfb, st->zb);
    for (int y = 0; y < RH; y++) {                                        /* 2x nearest blit */
        const u16 *s = st->rfb + y * RW; u32 *d0 = (u32 *)(st->cv + (2 * y) * KBD_W), *d1 = (u32 *)(st->cv + (2 * y + 1) * KBD_W);
        for (int x = 0; x < RW; x++) { u32 p = s[x]; p |= p << 16; d0[x] = p; d1[x] = p; }
    }
    st->frames++; if (now - st->fps_t >= 1000) { st->fps = st->frames; st->frames = 0; st->fps_t = now; }
    if (g->modified && ++st->save_t > 3600 * 2) { st->save_t = 0; mc_save(st); }               /* autosave every ~2 minutes of play */
    mc_hud(st); mc_controls(st); INVALIDATE(st->img);
}
static int game_open(void *root, int from_menu) {
    GSt *st = (GSt *)MALLOC(sizeof(GSt)); if (!st) return 0;
    u32 *cvblk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); u16 *rfb = (u16 *)MALLOC(RW * RH * 2), *zb = (u16 *)MALLOC(RW * RH * 2), *tp = (u16 *)MALLOC(VX_TEXW * 16 * 2); Vox *g = (Vox *)MALLOC(sizeof(Vox));
    if (!cvblk || !rfb || !zb || !tp || !g) { if (cvblk) FREE(cvblk); if (rfb) FREE(rfb); if (zb) FREE(zb); if (tp) FREE(tp); if (g) FREE(g); FREE(st); return 0; }
    for (unsigned i = 0; i < sizeof *st; i++) ((u8 *)st)[i] = 0;
    vx_make_tex(tp); u32 now = TICK_GET(); g->modified = 0; g->flying = 1; g->sel = 0; g->survival = 0; g->dead = 0; g->hurt = 0;
    for (int i = 0; i < VB_N + 1; i++) g->inv[i] = 0;
    for (int i = 0; i < VX_MOBS; i++) g->mobs[i].type = VM_NONE;
    st->root = root; st->from_menu = from_menu; st->g = g; st->rfb = rfb; st->zb = zb; st->cvblk = cvblk; st->cv = (u16 *)((u8 *)cvblk + 8); st->texpix = tp;
    st->tex.pix = tp; st->tex.w = VX_TEXW; st->tex.h = 16; st->fps_t = now; st->seed = now * 2654435761u + 977u; st->wtype = 0; st->has_save = mc_probe(); st->screen = MS_MENU; st->dirty = 1;
    for (int i = 0; i < 9; i++) g->hot[i] = (u8)(i + 1);
    mc_menu_draw(st); st->dirty = 0;
    st->img = make_canvas(root, st->dsc, st->cv, (void *)game_event, st);
    st->timer = TIMER_CREATE((void *)game_tick, 33, st);
    return 1;
}
