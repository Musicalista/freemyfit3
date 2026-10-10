/* ================= "Meus apps": an app store client for the watch (original code) =================
 * Apps are small native programs (apps/f3app.h, built with apps/appcc.py) kept as files in /user/apps/<id>.f3a. The list lives in /user/apps/index.txt, one line per app:
 *     id;BGRRGGBB;FGRRGGBB;Name in Portuguese;Name in English[;icon]  (colours of the tile as RRGGBB hex; the optional icon is 256 hex characters = a 32x32 one-bit picture, 4 bytes per row, MSB left, drawn in the BG colour on the FG tile)
 * Fit3 Manager (the PC program) writes both, so apps are installed in seconds without reflashing the firmware. This file lists the apps, loads the chosen one into a RAM block
 * (checks magic/size/CRC32, applies the relocation table), calls its f3app_main() and then its tick() every 33 ms. The app draws on a 256x402 canvas; we draw the X button on top.
 * NOTE: it runs code that was copied into RAM: if the watch refuses to execute from RAM the app crashes (the watch reboots); nothing is written to flash.
 * Needs apps/f3app.h on the include path, common.inc.c (mg_num) and the drawing helpers of kbd.c. */
#include "f3app.h"
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
#define AS_MAX 12
#define AS_INDEX "/user/apps/index.txt"
typedef struct {
    u32 dsc[4]; u32 *cvblk; u16 *px; void *root, *img, *timer; int from_menu, running, page, dirty, pressed, x, y, tap, napps, err;
    char id[AS_MAX][14], npt[AS_MAX][18], nen[AS_MAX][18]; u16 bg[AS_MAX], fg[AS_MAX]; int cur; u8 ico[AS_MAX][128], hasico[AS_MAX];
    u8 *code; F3App *app; F3Api api;
} AS;

static int menu_open(void *root);
static void *as_malloc(u32 n) { return MALLOC(n); }
static void as_free(void *p) { FREE(p); }
static u32 as_ticks(void) { return TICK_GET(); }
static void as_vib(int id) { MOTOR_ONCE(id, 0); }
#ifndef RTC_GET
#define RTC_GET ((void (*)(void *))0x2c0cb8f5)                    /* fills 9 ints: [1] sec [2] min [3] hour [4] day [5] month [6] year [7] weekday (layout from AT^DATETIME?) */
#endif
#ifndef BAT_GET
#define BAT_GET ((int (*)(int, int *))0x2c0a9e25)                 /* power info by type; type 1 = battery percent (as AT^GETBATPERCENT?) */
#endif
static void as_time(F3Time *t) { int w[9]; for (int i = 0; i < 9; i++) w[i] = 0; RTC_GET(w); t->sec = w[1]; t->min = w[2]; t->hour = w[3]; t->day = w[4]; t->month = w[5]; t->year = w[6]; t->wday = w[7]; }
static int as_battery(void) { int v = 0; BAT_GET(1, &v); return v < 0 ? 0 : v > 100 ? 100 : v; }
static int as_fopen(const char *p, const char *m) { return FS_OPEN(p, m); }
static int as_fread(int fd, void *b, u32 n) { return FS_READ(fd, b, n); }
static int as_fwrite(int fd, const void *b, u32 n) { return FS_WRITE(fd, b, n); }
static int as_fclose(int fd) { return FS_CLOSE(fd); }
#ifndef AS_CALL
#define AS_CALL(a, fn) ((fn)(&(a)->api))
#endif

static u32 as_crc32(const u8 *b, u32 n) {
    u32 c = 0xffffffffu; for (u32 i = 0; i < n; i++) { c ^= b[i]; for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xedb88320u : c >> 1; } return ~c;
}
static int as_hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0; }
static u16 as_rgb(const char *h) { int r = as_hexv(h[0]) * 16 + as_hexv(h[1]), g = as_hexv(h[2]) * 16 + as_hexv(h[3]), b = as_hexv(h[4]) * 16 + as_hexv(h[5]); return C(r, g, b); }
static void as_read_index(AS *a) {
    a->napps = 0; int fd = FS_OPEN(AS_INDEX, "r"); if (fd < 0) return;
    const int cap = 4600; char *buf = (char *)MALLOC((u32)cap); if (!buf) { FS_CLOSE(fd); return; }
    int n = 0, r; while (n < cap - 1 && (r = FS_READ(fd, buf + n, (u32)(cap - 1 - n))) > 0) n += r; FS_CLOSE(fd); buf[n] = 0;
    int i = 0;
    while (i < n && a->napps < AS_MAX) {
        char *f[6]; int nf = 0, ok = 1; f[nf++] = buf + i; f[5] = buf + n;
        while (i < n && buf[i] != '\n') { if (buf[i] == ';' && nf < 6) { buf[i] = 0; f[nf++] = buf + i + 1; } else if (buf[i] == '\r') buf[i] = 0; i++; }
        if (i < n) buf[i++] = 0;
        if (nf < 5) continue; int k = a->napps;
        for (int j = 0; f[0][j]; j++) if (j > 11 || !((f[0][j] >= 'a' && f[0][j] <= 'z') || (f[0][j] >= '0' && f[0][j] <= '9') || f[0][j] == '_' || f[0][j] == '-')) ok = 0;     /* ids become file names: keep them plain */
        if (!ok || !f[0][0] || !f[1][0] || !f[1][1] || !f[1][2] || !f[1][3] || !f[1][4] || !f[1][5] || !f[2][0] || !f[2][1] || !f[2][2] || !f[2][3] || !f[2][4] || !f[2][5]) continue;
        { int j = 0; while (f[0][j]) { a->id[k][j] = f[0][j]; j++; } a->id[k][j] = 0; }
        a->bg[k] = as_rgb(f[1]); a->fg[k] = as_rgb(f[2]);
        { int j = 0; while (f[3][j] && j < 17) { a->npt[k][j] = f[3][j]; j++; } a->npt[k][j] = 0; j = 0; while (f[4][j] && j < 17) { a->nen[k][j] = f[4][j]; j++; } a->nen[k][j] = 0; }
        a->hasico[k] = 0;
        if (nf >= 6) { int j = 0; for (; j < 256 && ((f[5][j] >= '0' && f[5][j] <= '9') || (f[5][j] >= 'A' && f[5][j] <= 'F') || (f[5][j] >= 'a' && f[5][j] <= 'f')); j++) ; if (j == 256) { for (int q = 0; q < 128; q++) a->ico[k][q] = (u8)(as_hexv(f[5][2 * q]) * 16 + as_hexv(f[5][2 * q + 1])); a->hasico[k] = 1; } }
        a->napps++;
    }
    FREE(buf);
}

/* ---------------- loading ---------------- */
static void as_stop(AS *a) {
    if (a->app && a->app->close) a->app->close(a->app);
    a->app = 0; if (a->code) { FREE(a->code); a->code = 0; } a->running = 0; a->dirty = 1; a->pressed = a->tap = 0;
}
static u32 as_u32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static int as_load(AS *a, int idx) {                                              /* 0 ok; otherwise a message code */
    char path[40]; int n = 0; const char *d = "/user/apps/"; while (*d) path[n++] = *d++; for (int j = 0; a->id[idx][j]; j++) path[n++] = a->id[idx][j]; path[n++] = '.'; path[n++] = 'f'; path[n++] = '3'; path[n++] = 'a'; path[n] = 0;
    int fd = FS_OPEN(path, "r"); if (fd < 0) return 1;
    u8 h[64]; int got = 0, r; while (got < 64 && (r = FS_READ(fd, h + got, (u32)(64 - got))) > 0) got += r;
    if (got < 64 || as_u32(h) != 0x31413346u || (h[4] | (h[5] << 8)) != F3_API_VERSION) { FS_CLOSE(fd); return 2; }
    u32 img = as_u32(h + 8), bss = as_u32(h + 12), entry = as_u32(h + 16), crc = as_u32(h + 20), nrel = as_u32(h + 24);
    if (img < 16 || img > 40000 || bss > 16384 || entry >= img || (entry & 1) || nrel > 4096) { FS_CLOSE(fd); return 2; }
    u32 total = img + 4 * nrel; u8 *blob = (u8 *)MALLOC(total + 4); if (!blob) { FS_CLOSE(fd); return 3; }
    got = 0; while ((u32)got < total && (r = FS_READ(fd, blob + got, total - (u32)got)) > 0) got += r; FS_CLOSE(fd);
    if ((u32)got < total || as_crc32(blob, img) != crc) { FREE(blob); return 2; }
    u32 mem = (img + bss + 8) & ~3u; u8 *code = (u8 *)MALLOC(mem + 8); if (!code) { FREE(blob); return 3; }
    for (u32 i = 0; i < mem; i++) code[i] = i < img ? blob[i] : 0;
    for (u32 i = 0; i < nrel; i++) {
        u32 off = as_u32(blob + img + 4 * i); if ((off & 3) || off + 4 > img) { FREE(blob); FREE(code); return 2; }
        u32 v = as_u32(code + off) + (u32)code; if (v < (u32)code || v > (u32)code + mem) { FREE(blob); FREE(code); return 2; }        /* every relocated word must point inside the app */
        code[off] = (u8)v; code[off + 1] = (u8)(v >> 8); code[off + 2] = (u8)(v >> 16); code[off + 3] = (u8)(v >> 24);
    }
    FREE(blob);
#ifdef __arm__
    __asm__ volatile("dsb\n\tisb" ::: "memory");
#endif
    a->code = code; a->cur = idx;
    F3App *(*fn)(const F3Api *) = (F3App *(*)(const F3Api *))(void *)(((u32)code + entry) | 1u);
    a->app = AS_CALL(a, fn);
    if (!a->app) { FREE(code); a->code = 0; return 4; }
    return 0;
}

/* ---------------- UI ---------------- */
static void as_tile_rect(int i, int *x, int *y, int *w, int *h) { int pos = i % 6; *x = 10 + (pos % 2) * 120; *y = 50 + (pos / 2) * 102; *w = 116; *h = 94; }
static void as_draw_list(AS *a) {
    u16 *g = a->px; rect(g, 0, 0, KBD_W, KBD_H, P_BG); ptext(g, 14, 8, TR("Meus apps", "My apps"), 2, P_TEXT); rect(g, 216, 0, 40, 28, C(170, 30, 30)); put(g, 228, 8, 'X', 2, C(255, 255, 255));
    if (!a->napps) { draw_wrapped(g, TR("Nenhum app instalado.\n\nNo PC, abra o Fit3 Manager, conecte o relogio e instale apps pela Loja de apps (aba Apps leves).", "No apps installed.\n\nOn the PC, open Fit3 Manager, connect the watch and install apps from the app store (Light apps tab)."), 14, 60, 1, 28, 12, 0, P_TEXT, 0); return; }
    for (int j = 0; j < 6; j++) {
        int i = a->page * 6 + j; if (i >= a->napps) break; int x, y, w, h; as_tile_rect(i, &x, &y, &w, &h);
        card(g, x, y, w, h, 28, a->bg[i]); rrect(g, x + w / 2 - 18, y + 14, 36, 36, 10, a->fg[i]); { const char *s = TR(a->npt[i], a->nen[i]); char ini[2] = { s[0], 0 };
          if (a->hasico[i]) { int ox = x + w / 2 - 16, oy = y + 16; for (int r = 0; r < 32; r++) for (int c = 0; c < 32; c++) if (a->ico[i][r * 4 + c / 8] & (0x80 >> (c & 7))) g[(oy + r) * KBD_W + ox + c] = a->bg[i]; } else ctext(g, x + w / 2, y + 22, ini, 2, a->bg[i]);
          int ln = 0; while (s[ln]) ln++; ctext(g, x + w / 2, y + h - (ln > 7 ? 24 : 30), s, ln > 7 ? 1 : 2, a->fg[i]); }
    }
    if (a->napps > 6) { card(g, 10, 358, 76, 34, 17, a->page > 0 ? P_CARD : C(236, 230, 244)); ctext(g, 48, 361, "<", 3, P_ACCENT); card(g, 170, 358, 76, 34, 17, (a->page + 1) * 6 < a->napps ? P_CARD : C(236, 230, 244)); ctext(g, 208, 361, ">", 3, P_ACCENT); }
    if (a->err) draw_wrapped(g, a->err == 1 ? TR("Arquivo do app nao encontrado.", "App file not found.") : a->err == 2 ? TR("Arquivo do app invalido (reinstale).", "Invalid app file (reinstall).") : a->err == 3 ? TR("Sem memoria para o app.", "Not enough memory for the app.") : TR("O app nao iniciou.", "The app did not start."), 12, 338, 1, 30, 2, 0, C(190, 40, 40), 0);
}
static void as_close(AS *a) {
    as_stop(a); TIMER_DEL(a->timer); ADD_FLAG(a->img, 1); void *root = a->root; int fm = a->from_menu; FREE(a->cvblk); FREE(a); if (fm) menu_open(root);
}
static void as_event(void *e) {
    int code = EV_CODE(e), x, y; AS *a = (AS *)EV_USER(e);
    if (code == 1 || code == 2) { if (touch_read(&x, &y)) { a->pressed = 1; a->x = x; a->y = y; } return; }
    if (code == 8 || code == 3) { a->pressed = 0; return; }
    if (code != 7) return;
    if (a->x >= 216 && a->y < 28) { if (a->running) as_stop(a); else as_close(a); return; }
    if (a->running) { a->tap = 1; return; }
    if (a->napps > 6 && a->y >= 356) { if (a->x < 100 && a->page > 0) a->page--; else if (a->x >= 156 && (a->page + 1) * 6 < a->napps) a->page++; a->dirty = 1; return; }
    for (int j = 0; j < 6; j++) {
        int i = a->page * 6 + j; if (i >= a->napps) break; int tx, ty, tw, th; as_tile_rect(i, &tx, &ty, &tw, &th);
        if (a->x >= tx && a->x < tx + tw && a->y >= ty && a->y < ty + th) { a->err = as_load(a, i); if (!a->err) { a->running = 1; a->tap = 0; } a->dirty = 1; return; }
    }
}
static void as_tick(void *timer) {
    AS *a = *(AS **)((u8 *)timer + 0xC);
    if (a->running && a->app) {
        int tap = a->tap; a->tap = 0; int r = a->app->tick(a->app, a->pressed, a->x, a->y, tap);
        rect(a->px, 216, 0, 40, 28, C(170, 30, 30)); put(a->px, 228, 8, 'X', 2, C(255, 255, 255)); INVALIDATE(a->img);
        if (!r) as_stop(a);
        return;
    }
    if (a->dirty) { a->dirty = 0; as_draw_list(a); INVALIDATE(a->img); }
}
static int as_open(void *root, int from_menu) {
#ifdef HAVE_RECOVERY
    if (rc_safe()) return rc_open(root, from_menu);                             /* safe mode: no app is loaded; the recovery screen lets you turn it off */
#endif
    AS *a = (AS *)MALLOC(sizeof(AS)); if (!a) return 0;
    u32 *blk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!blk) { FREE(a); return 0; }
    for (unsigned i = 0; i < sizeof *a; i++) ((u8 *)a)[i] = 0;
    a->cvblk = blk; a->px = (u16 *)((u8 *)blk + 8); a->root = root; a->from_menu = from_menu;
    a->api.version = F3_API_VERSION; a->api.fb = a->px; a->api.w = KBD_W; a->api.h = KBD_H; a->api.malloc = as_malloc; a->api.free = as_free; a->api.ticks = as_ticks; a->api.vibrate = as_vib;
    a->api.rect = rect; a->api.rrect = rrect; a->api.disc = disc; a->api.text = ptext; a->api.ctext = ctext; a->api.num = mg_num;
    a->api.fopen = as_fopen; a->api.fread = as_fread; a->api.fwrite = as_fwrite; a->api.fclose = as_fclose; a->api.lang_pt = (u32)ui_pt(); a->api.time = as_time; a->api.battery = as_battery;
    as_read_index(a); as_draw_list(a);
    a->img = make_canvas(root, a->dsc, a->px, (void *)as_event, a);
    a->timer = TIMER_CREATE((void *)as_tick, 33, a);
    return 1;
}
