/* ================= Recovery screen (original code) =================
 * A page of "Apps extras" with the few things that help when a mod misbehaves, none of which touches the firmware image itself:
 *   - Safe mode: a flag file (/user/safe.flag) that keeps "Meus apps" from loading and running any app (opening it shows this screen instead);
 *   - clear the apps list (/user/apps/index.txt is emptied), reset the apps' best scores (/user/*.sav of the light apps);
 *   - how to go back to the original firmware, and the one supported model and version.
 * Destructive buttons need two taps within 4 seconds. This does NOT rescue a watch that cannot boot: that needs the stock firmware and its own recovery.
 * Needs the drawing helpers of kbd.c and common.inc.c; the file API is the watch's own (same table as appsys.inc.c). */
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
#define RC_SAFE "/user/safe.flag"
typedef struct { u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img; int from_menu, x, y, arm; u32 arm_t; int msg; } RC;   /* msg: 0 none, 1..4 result/help texts */

static int menu_open(void *root);
static int rc_safe(void) { int fd = FS_OPEN(RC_SAFE, "r"); if (fd < 0) return 0; char c = 0; int r = FS_READ(fd, &c, 1); FS_CLOSE(fd); return r == 1 && c == '1'; }
static void rc_set_safe(int on) { int fd = FS_OPEN(RC_SAFE, "w"); if (fd < 0) return; char c = on ? '1' : '0'; FS_WRITE(fd, &c, 1); FS_CLOSE(fd); }
static void rc_empty(const char *path) { int fd = FS_OPEN(path, "r"); if (fd < 0) return; FS_CLOSE(fd); fd = FS_OPEN(path, "w"); if (fd >= 0) FS_CLOSE(fd); }   /* only files that exist; "w" truncates */
static const char *const rc_sav[] = { "snake", "flappy", "tetris", "g2048", "breakout", "simon", "reflexo", "memoria", "relogio", "alarme", "contador", "meujogo" };
static void rc_clear_scores(void) {
    for (unsigned i = 0; i < sizeof rc_sav / sizeof rc_sav[0]; i++) { char p[32]; int n = 0; const char *d = "/user/"; while (*d) p[n++] = *d++; for (const char *q = rc_sav[i]; *q; q++) p[n++] = *q; p[n++] = '.'; p[n++] = 's'; p[n++] = 'a'; p[n++] = 'v'; p[n] = 0; rc_empty(p); }
}
static void rc_btn(u16 *g, int y, const char *s, int armed, int danger) {
    card(g, 12, y, 232, 42, 16, armed ? C(236, 112, 100) : P_CARD);
    if (armed) ctext(g, 128, y + 13, TR("Toque de novo", "Tap again"), 2, C(255, 255, 255)); else ctext(g, 128, y + 14, s, 1, danger ? C(176, 52, 52) : P_TEXT);
}
static void rc_draw(RC *a) {
    u16 *g = a->px; rect(g, 0, 0, KBD_W, KBD_H, P_BG); ptext(g, 14, 8, TR("Recuperacao", "Recovery"), 2, P_TEXT); rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255));
    card(g, 8, 36, 240, 92, 18, P_CARD);
    draw_wrapped(g, TR("Suportado apenas:\nGalaxy Fit3 SM-R390,\nfirmware R390XXU0AZA3.\nOutros: risco de loop.", "Supported only:\nGalaxy Fit3 SM-R390,\nfirmware R390XXU0AZA3.\nOthers: reboot-loop risk."), 18, 44, 1, 26, 4, 0, P_TEXT, 0);
    ptext(g, 18, 106, rc_safe() ? TR("Modo seguro: LIGADO", "Safe mode: ON") : TR("Modo seguro: desligado", "Safe mode: off"), 1, rc_safe() ? C(176, 52, 52) : C(40, 120, 84));
    rc_btn(g, 138, rc_safe() ? TR("Desligar modo seguro", "Turn safe mode off") : TR("Ligar modo seguro", "Turn safe mode on"), 0, 0);
    rc_btn(g, 186, TR("Limpar lista de apps", "Clear the apps list"), a->arm == 1, 1);
    rc_btn(g, 234, TR("Zerar recordes dos apps", "Reset the apps' best scores"), a->arm == 2, 1);
    rc_btn(g, 282, TR("Como voltar ao original", "How to go back to stock"), 0, 0);
    if (a->msg == 1) draw_wrapped(g, TR("Pronto: lista de apps limpa.", "Done: apps list cleared."), 14, 336, 1, 28, 2, 0, C(40, 120, 84), 0);
    if (a->msg == 2) draw_wrapped(g, TR("Pronto: recordes zerados.", "Done: best scores reset."), 14, 336, 1, 28, 2, 0, C(40, 120, 84), 0);
    if (a->msg == 3) draw_wrapped(g, TR("Modo seguro mudou.\nLigado: o Meus apps nao\ncarrega apps.", "Safe mode changed.\nWhen on, My apps loads\nno apps."), 14, 336, 1, 28, 3, 0, P_TEXT, 0);
    if (a->msg == 4) draw_wrapped(g, TR("No PC: Fit3 Manager >\nLoja de apps > Original\nSamsung (restaurar), ou\ngrave o stock-aza3.bin.\nRelogio com 50% ou mais\nde bateria.", "On the PC: Fit3 Manager >\nApp store > Original\nSamsung (restore), or flash\nstock-aza3.bin. Keep the\nwatch at 50% or more."), 14, 324, 1, 28, 6, 0, P_TEXT, 0);
}
static void rc_close(RC *a) { ADD_FLAG(a->img, 1); void *root = a->root; int fm = a->from_menu; FREE(a->cvblk); FREE(a); if (fm) menu_open(root); }
static void rc_event(void *e) {
    int code = EV_CODE(e), x, y; RC *a = (RC *)EV_USER(e);
    if (code == 1 || code == 2) { if (touch_read(&x, &y)) { a->x = x; a->y = y; } return; }
    if (code != 7) return;
    if (a->x >= 216 && a->y < 28) { rc_close(a); return; }
    u32 now = TICK_GET(); if (a->arm && now - a->arm_t > 4000u) a->arm = 0;
    int b = a->y >= 138 && a->y < 180 ? 0 : a->y >= 186 && a->y < 228 ? 1 : a->y >= 234 && a->y < 276 ? 2 : a->y >= 282 && a->y < 324 ? 3 : -1;
    if (b < 0) return;
    a->msg = 0;
    if (b == 0) { rc_set_safe(!rc_safe()); a->arm = 0; a->msg = 3; MOTOR_ONCE(1, 0); }
    else if (b == 3) { a->arm = 0; a->msg = 4; }
    else if (a->arm == b) { a->arm = 0; if (b == 1) { int fd = FS_OPEN("/user/apps/index.txt", "w"); if (fd >= 0) { FS_WRITE(fd, "\n", 1); FS_CLOSE(fd); } a->msg = 1; } else { rc_clear_scores(); a->msg = 2; } MOTOR_ONCE(4, 0); }
    else { a->arm = b; a->arm_t = now; MOTOR_ONCE(1, 0); }
    rc_draw(a); INVALIDATE(a->img);
}
static int rc_open(void *root, int from_menu) {
    RC *a = (RC *)MALLOC(sizeof(RC)); if (!a) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!blk) { FREE(a); return 0; }
    for (unsigned i = 0; i < sizeof *a; i++) ((u8 *)a)[i] = 0;
    a->cvblk = blk; a->px = (u16 *)((u8 *)blk + 8); a->root = root; a->from_menu = from_menu;
    rc_draw(a); a->img = make_canvas(root, a->dsc, a->px, (void *)rc_event, a);
    return 1;
}
