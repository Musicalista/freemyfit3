/* qemu_app.c: runs the real j2me_app.c (the watch app) on the bare-metal Cortex-M4 with a fake F3Api: in-memory files, a 256x402 framebuffer, scripted touches. */
#include <stdint.h>
#include <stddef.h>
#include "f3app.h"
extern uint8_t list_data[], list_end[], jar_data[], jar_end[], rt_data[], rt_end[];
extern uint8_t __arena[];
extern uint8_t reset_handler[];

static int sh(int op, void *arg) { register int r0 __asm__("r0") = op; register void *r1 __asm__("r1") = arg; __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory"); return r0; }
static void out(const char *s) { sh(4, (void *)s); }
static void outnum(const char *label, uint32_t v) { char b[40]; int n = 0; uint32_t d[10]; int nd = 0; out(label); if (!v) d[nd++] = 0; while (v) { d[nd++] = v % 10; v /= 10; } while (nd) b[n++] = (char)('0' + d[--nd]); b[n++] = 10; b[n] = 0; out(b); }
static uint8_t *arena_top, *arena_end;
static void *a_malloc(u32 n) { n = (n + 7) & ~7u; if (arena_top + n > arena_end) return 0; void *p = arena_top; arena_top += n; return p; }
static void a_free(void *p) { (void)p; }
static uint32_t vt = 1000;
static u32 a_ticks(void) { return vt; }
static void a_vib(int id) { outnum("[vibrate] ", (uint32_t)id); }
/* fake files */
typedef struct { const char *name; const uint8_t *p; uint32_t n; uint32_t pos; } FF;
static FF files[3]; static int open_fd = -1;
static int a_fopen(const char *path, const char *mode) {
    if (mode[0] == 'w') return -1;
    for (int i = 0; i < 3; i++) { const char *a = files[i].name, *b = path; while (*a && *a == *b) { a++; b++; } if (!*a && !*b) { files[i].pos = 0; return i; } }
    return -1;
}
static int a_fread(int fd, void *buf, u32 n) { FF *f = &files[fd]; uint32_t r = f->n - f->pos; if (r > n) r = n; uint8_t *d = (uint8_t *)buf; for (uint32_t i = 0; i < r; i++) d[i] = f->p[f->pos + i]; f->pos += r; return (int)r; }
static int a_fwrite(int fd, const void *b, u32 n) { (void)fd; (void)b; return (int)n; }
static int a_fclose(int fd) { (void)fd; return 0; }
static void a_rect(u16 *fb, int x, int y, int w, int h, u16 c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < 256 && j >= 0 && j < 402) fb[j * 256 + i] = c; }
static void a_rrect(u16 *fb, int x, int y, int w, int h, int r, u16 c) { (void)r; a_rect(fb, x, y, w, h, c); }
static void a_disc(u16 *fb, int cx, int cy, int r, u16 c) { a_rect(fb, cx - r, cy - r, 2 * r, 2 * r, c); }
static void a_text(u16 *fb, int x, int y, const char *s, int sc, u16 c) { int n = 0; while (s[n]) n++; a_rect(fb, x, y, n * 8 * sc, 12 * sc, c); }
static void a_ctext(u16 *fb, int cx, int y, const char *s, int sc, u16 c) { int n = 0; while (s[n]) n++; a_rect(fb, cx - n * 4 * sc, y, n * 8 * sc, 12 * sc, c); }
static void a_num(u16 *fb, int x, int y, int v, int sc, u16 c) { (void)v; a_rect(fb, x, y, 8 * sc, 12 * sc, c); }
static u16 fbuf[256 * 402];
int main(void) {
    arena_top = __arena; arena_end = (uint8_t *)0x20400000 - 64 * 1024;
    files[0].name = "/user/apps/jm_list.txt"; files[0].p = list_data; files[0].n = (uint32_t)(list_end - list_data);
    files[1].name = "/user/apps/jm_rt.pak"; files[1].p = rt_data; files[1].n = (uint32_t)(rt_end - rt_data);
    files[2].name = "/user/apps/test.jar"; files[2].p = jar_data; files[2].n = (uint32_t)(jar_end - jar_data);
    static F3Api api; api.version = 1; api.fb = fbuf; api.w = 256; api.h = 402; api.malloc = a_malloc; api.free = a_free; api.ticks = a_ticks; api.vibrate = a_vib;
    api.rect = a_rect; api.rrect = a_rrect; api.disc = a_disc; api.text = a_text; api.ctext = a_ctext; api.num = a_num;
    api.fopen = a_fopen; api.fread = a_fread; api.fwrite = a_fwrite; api.fclose = a_fclose; api.lang_pt = 1;
    F3App *app = f3app_main(&api);
    if (!app) { out("f3app_main failed\n"); sh(0x18, (void *)0x20023); }
    for (int i = 0; i < 600; i++) {
        int pressed = 0, x = 0, y = 0;
        if (i >= 5 && i < 8) { pressed = 1; x = 100; y = 60; }                 /* tap the first game in the list */
        if (i >= 100 && i < 103) { pressed = 1; x = 120; y = 200; }            /* tap on the canvas */
        if (i >= 200 && i < 204) { pressed = 1; x = 17 + 4 * 31; y = 375; }    /* pad OK button */
        if (!app->tick(app, pressed, x, y, 0)) { out("app exited\n"); break; }
        vt += 33;
    }
    uint32_t sum = 0; for (int i = 0; i < 256 * 402; i++) sum = sum * 31 + fbuf[i];
    outnum("fb checksum ", sum);
    { uint32_t args[3] = { (uint32_t)"qemu_fb.raw", 5, 11 }; int fd = sh(1, args); if (fd >= 0) { uint32_t w[3] = { (uint32_t)fd, (uint32_t)fbuf, sizeof fbuf }; sh(5, w); uint32_t c[1] = { (uint32_t)fd }; sh(2, c); } }
    if (app->close) app->close(app);
    out("QEMU APP DONE\n"); sh(0x18, (void *)0x20026);
    for (;;) {}
}
