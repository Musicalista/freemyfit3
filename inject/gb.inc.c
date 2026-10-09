/* ================= Game Boy front end (uses gbcore.inc.c) =================
 * The cartridge is read from the watch file system: /user/gb.gb (.gb or .gbc). Battery saves go to /user/gb.sav (written every ~30 s when changed, and on exit).
 * Screen: 160x144 scaled 1.6x to 256x230 on top, touch controls below (the panel reports ONE finger: HOLD A / HOLD B latch the buttons, so
 * "run + move" and "A + direction" are possible). No sound. */
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
#define GB_ROM_PATH "/user/gb.gb"
#define GB_SAV_PATH "/user/gb.sav"
#define GB_SCR_H 230
#define GB_UI_Y 262

typedef struct {
    u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img, *timer; int from_menu;
    GB *g; u8 *rom; int err, pressed, x, y, lockA, lockB, uil, t0done, saved_t; u32 t0, done; u8 xm[256];
} GBA;

static void gb_ui_hit(GBA *a, u8 *jd, u8 *jb) {
    *jd = 0; *jb = (u8)((a->lockA ? 1 : 0) | (a->lockB ? 2 : 0));
    if (!a->pressed) return;
    int x = a->x, y = a->y, dx = x - 62, dy = y - 322;
    if (y >= GB_UI_Y && dx >= -62 && dx <= 62 && dy >= -60 && dy <= 60) { if (dx > 20) *jd |= 1; if (dx < -20) *jd |= 2; if (dy < -20) *jd |= 4; if (dy > 20) *jd |= 8; }
    if ((x - 222) * (x - 222) + (y - 312) * (y - 312) <= 34 * 34) *jb |= 1;
    if ((x - 170) * (x - 170) + (y - 340) * (y - 340) <= 34 * 34) *jb |= 2;
    if (y >= 374 && y < 402) { if (x >= 20 && x < 108) *jb |= 4; else if (x >= 148 && x < 236) *jb |= 8; }
}
static void gb_ui_draw(GBA *a, int jd, int jb) {
    u16 *g = a->px; uint16_t bg = C(14, 14, 24), key = C(70, 74, 96), on = C(150, 160, 210), led = C(120, 220, 140);
    rect(g, 0, GB_SCR_H, KBD_W, KBD_H - GB_SCR_H, bg);
    rrect(g, 6, 234, 66, 24, 6, a->lockB ? led : key); ctext(g, 39, 242, TR("SEGURA B", "HOLD B"), 1, a->lockB ? C(10, 40, 20) : C(220, 224, 240));
    rrect(g, 78, 234, 66, 24, 6, a->lockA ? led : key); ctext(g, 111, 242, TR("SEGURA A", "HOLD A"), 1, a->lockA ? C(10, 40, 20) : C(220, 224, 240));
    rrect(g, 150, 234, 52, 24, 6, key); ctext(g, 176, 242, TR("SALVAR", "SAVE"), 1, C(220, 224, 240));
    rrect(g, 212, 234, 38, 24, 6, C(170, 30, 30)); ctext(g, 231, 238, "X", 2, C(255, 255, 255));
    rrect(g, 45, 270, 34, 104, 8, (jd & 4) ? on : key); rrect(g, 10, 305, 104, 34, 8, key);
    rrect(g, 45, 270, 34, 52, 8, (jd & 4) ? on : key); rrect(g, 45, 322, 34, 52, 8, (jd & 8) ? on : key);
    rrect(g, 10, 305, 52, 34, 8, (jd & 2) ? on : key); rrect(g, 62, 305, 52, 34, 8, (jd & 1) ? on : key);
    rect(g, 45, 305, 34, 34, key); disc(g, 62, 322, 8, bg);
    disc(g, 170, 340, 26, (jb & 2) ? on : C(160, 60, 100)); ctext(g, 170, 332, "B", 2, C(255, 255, 255));
    disc(g, 222, 312, 26, (jb & 1) ? on : C(160, 60, 100)); ctext(g, 222, 304, "A", 2, C(255, 255, 255));
    rrect(g, 20, 378, 88, 20, 10, (jb & 4) ? on : key); ctext(g, 64, 383, "SELECT", 1, C(220, 224, 240));
    rrect(g, 148, 378, 88, 20, 10, (jb & 8) ? on : key); ctext(g, 192, 383, "START", 1, C(220, 224, 240));
}
static void gb_emit(GB *g, int ly) {                                    /* one finished scanline -> the scaled canvas rows it covers */
    GBA *a = (GBA *)g->emit_ctx; int ds = ly * GB_SCR_H / 144, de = (ly + 1) * GB_SCR_H / 144; u16 *d = a->px + ds * KBD_W;
    for (int x = 0; x < KBD_W; x++) d[x] = g->line[a->xm[x]];
    for (int r = ds + 1; r < de; r++) { u32 *s = (u32 *)d, *t = (u32 *)(a->px + r * KBD_W); for (int i = 0; i < KBD_W / 2; i++) t[i] = s[i]; }
}
static int gb_fread(const char *path, u8 *buf, int max) {
    int fd = FS_OPEN(path, "r"); if (fd < 0) return -1;
    int n = 0, r; while (n < max && (r = FS_READ(fd, buf + n, (u32)(max - n > 4096 ? 4096 : max - n))) > 0) n += r;
    FS_CLOSE(fd); return n;
}
static void gb_save(GBA *a) {
    GB *g = a->g; if (!g || !g->bat || !g->ersz || !g->dirty) return;
    int fd = FS_OPEN(GB_SAV_PATH, "w"); if (fd < 0) return;
    FS_WRITE(fd, g->eram, g->ersz); FS_CLOSE(fd); g->dirty = 0;
}
static void gb_tick(void *timer) {
    GBA *a = *(GBA **)((u8 *)timer + 0xC); GB *g = a->g; if (!g) return;
    u8 jd, jb; gb_ui_hit(a, &jd, &jb);
    if ((jd | jb) & ~(g->jd | g->jb)) g->io[0x0F] |= 0x10;               /* joypad interrupt on a new press */
    g->jd = jd; g->jb = jb;
    u32 now = TICK_GET(), total = (now - a->t0) * 597u / 10000u;
    if (total <= a->done) return;
    u32 due = total - a->done; if (due > 4) { a->done = total - 2; due = 2; }
    for (u32 i = 0; i < due; i++) gb_frame(g, i == due - 1);
    a->done += due;
    if (a->done > 100000) { a->t0 = now; a->done = 0; }
    { int key = jd | (jb << 4) | (a->lockA << 8) | (a->lockB << 9); if (key != a->uil) { a->uil = key; gb_ui_draw(a, jd, jb); } }
    if (g->dirty && ++a->saved_t > 1800) { a->saved_t = 0; gb_save(a); }
    INVALIDATE(a->img);
}
static void gb_free(GBA *a) {
    if (a->g) { gb_save(a); FREE(a->g->eram); FREE(a->g); a->g = 0; }
    FREE(a->rom); a->rom = 0;
}
static int menu_open(void *root);
static void gb_close(GBA *a) { if (a->timer) TIMER_DEL(a->timer); ADD_FLAG(a->img, 1); gb_free(a); FREE(a->cvblk); if (a->from_menu) menu_open(a->root); }
static void gb_event(void *e) {
    int code = EV_CODE(e), x, y; GBA *a = (GBA *)EV_USER(e);
    if (code == 1 || code == 2) { if (touch_read(&x, &y)) { a->pressed = 1; a->x = x; a->y = y; } return; }
    if (code == 8 || code == 3) { a->pressed = 0; return; }
    if (code != 7) return;                                                  /* tap: close / latch buttons / save */
    if (a->err) { if (a->x >= 200 && a->y < 40) gb_close(a); return; }
    if (a->y >= 230 && a->y < 262) {
        if (a->x >= 205) gb_close(a);
        else if (a->x >= 148) { if (a->g) { a->g->dirty = a->g->dirty || a->g->bat; gb_save(a); MOTOR_ONCE(1, 0); } }
        else if (a->x >= 76) a->lockA ^= 1;
        else if (a->x < 76) a->lockB ^= 1;
        a->uil = -1;
    }
}
static int gb_open(void *root, int from_menu) {
    GBA *a = (GBA *)MALLOC(sizeof(GBA)); if (!a) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!blk) { FREE(a); return 0; }
    gb_zero(a, sizeof *a); a->cvblk = blk; a->px = (u16 *)((u8 *)blk + 8); a->root = root; a->from_menu = from_menu; a->uil = -1;
    for (int i = 0; i < KBD_W * KBD_H; i++) a->px[i] = 0;
    for (int x = 0; x < 256; x++) a->xm[x] = (u8)(x * 10 / 16);
    {   /* load the cartridge: header first (it tells the size), then the rest in 4 KB blocks */
        u8 hdr[0x150]; int fd = FS_OPEN(GB_ROM_PATH, "r");
        if (fd < 0) a->err = 1;
        else {
            int n = 0, r; while (n < 0x150 && (r = FS_READ(fd, hdr + n, (u32)(0x150 - n))) > 0) n += r;
            if (n < 0x150 || hdr[0x148] > 8) a->err = 3;
            else {
                u32 size = 0x8000u << hdr[0x148]; a->rom = (u8 *)MALLOC(size);
                if (!a->rom) a->err = 2;
                else {
                    for (int i = 0; i < 0x150; i++) a->rom[i] = hdr[i];
                    u32 got = 0x150; while (got < size && (r = FS_READ(fd, a->rom + got, size - got > 4096 ? 4096 : size - got)) > 0) got += (u32)r;
                    for (u32 i = got; i < size; i++) a->rom[i] = 0xFF;
                    a->g = gb_new(a->rom, size); if (!a->g) { a->err = 2; FREE(a->rom); a->rom = 0; }
                }
            }
            FS_CLOSE(fd);
        }
    }
    if (a->g) {
        a->g->emit_ctx = a;
        if (a->g->bat && a->g->ersz) gb_fread(GB_SAV_PATH, a->g->eram, (int)a->g->ersz);
        a->t0 = TICK_GET();
    } else {
        u16 *g = a->px; rect(g, 0, 0, KBD_W, KBD_H, C(14, 14, 24)); rrect(g, 212, 6, 38, 24, 6, C(170, 30, 30)); ctext(g, 231, 10, "X", 2, C(255, 255, 255));
        ptext(g, 12, 12, "Game Boy", 2, C(150, 220, 140));
        draw_wrapped(g, a->err == 1 ? TR("Nenhum jogo encontrado.\n\nEnvie uma ROM (.gb ou .gbc) para o relogio em\n" GB_ROM_PATH "\n\nUse a pagina webbridge/bridge.html (botao 'Enviar ROM') ou o fit3-flasher.",
                                          "No game found.\n\nSend a ROM (.gb or .gbc) to the watch as\n" GB_ROM_PATH "\n\nUse webbridge/bridge.html ('Send ROM' button) or fit3-flasher.")
                     : a->err == 2 ? TR("Sem memoria para esta ROM.", "Not enough memory for this ROM.") : TR("ROM invalida.", "Invalid ROM."),
                     12, 48, 1, 29, 20, 0, C(220, 224, 240), 0);
    }
    a->img = make_canvas(root, a->dsc, a->px, (void *)gb_event, a);
    if (a->g) a->timer = TIMER_CREATE((void *)gb_tick, 16, a);
    return 1;
}
