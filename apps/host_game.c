/* PC harness for the Fit3 game apps: runs a game's f3app_main/tick against a software F3Api (clipping + out-of-range detection, 8x12 font), drives it with a tiny
 * input script and writes PPM screenshots.   host_game <prefix> "<script>" [monkey-seed]
 * script tokens: tN idle ticks | pX,Y press+release (tap) | hX,Y:N hold N ticks | sX1,Y1>X2,Y2 swipe | shot | lang0/lang1 | cH:M:S set the RTC | bN battery percent | expN expect vibrate count >= N so far
 * Compile the app with -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 (like appcc.py) so accented strings are Latin-1 bytes, as on the watch.
 * (a "monkey" seed instead runs 6000 random taps/swipes/holds and only checks that nothing crashes or draws outside the canvas) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kbd_font.h"
#include "f3app.h"
static u16 fb[F3_W * F3_H]; static unsigned now = 1000; static int vib, oob, calls; static char hsdir[256];
static void *h_malloc(u32 n) { return calloc(1, n + 16); }
static void h_free(void *p) { free(p); }
static u32 h_ticks(void) { return now; }
static void h_vib(int id) { (void)id; vib++; }
static void put(u16 *f, int x, int y, u16 c) { if (x >= 0 && y >= 0 && x < F3_W && y < F3_H) f[y * F3_W + x] = c; }   /* the watch's drawing functions clip too */
static void h_rect(u16 *f, int x, int y, int w, int h, u16 c) { calls++; if (w < 0 || h < 0) oob++; for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) put(f, x + i, y + j, c); }
static void h_rrect(u16 *f, int x, int y, int w, int h, int r, u16 c) {
    calls++; if (r * 2 > w || r * 2 > h) r = (w < h ? w : h) / 2;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) { int dx = i < r ? r - i : i >= w - r ? i - (w - r - 1) : 0, dy = j < r ? r - j : j >= h - r ? j - (h - r - 1) : 0; if (dx * dx + dy * dy > r * r + r) continue; put(f, x + i, y + j, c); }
}
static void h_disc(u16 *f, int cx, int cy, int r, u16 c) { calls++; for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r) put(f, cx + i, cy + j, c); }
static void h_text(u16 *f, int x, int y, const char *s, int sc, u16 c) {
    calls++; if (sc < 1 || sc > 12) oob++;
    for (; *s; s++, x += 8 * sc) {
        int cp = (unsigned char)*s; const uint8_t *g = (cp >= 32 && cp <= 126) ? kbd_font[cp - 32] : kbd_font['?' - 32];
        if (cp > 126) for (int e = 0; e < (int)(sizeof kbd_font_extra_cp / 2); e++) if (kbd_font_extra_cp[e] == cp) g = kbd_font[KBD_FONT_N_ASCII + e];
        for (int r = 0; r < 12; r++) for (int b = 0; b < 8; b++) if (g[r] & (0x80 >> b)) for (int j = 0; j < sc; j++) for (int i = 0; i < sc; i++) put(f, x + b * sc + i, y + r * sc + j, c);
    }
}
static void h_ctext(u16 *f, int cx, int y, const char *s, int sc, u16 c) { h_text(f, cx - (int)strlen(s) * 4 * sc, y, s, sc, c); }
static void h_num(u16 *f, int x, int y, int v, int sc, u16 c) { char b[16]; snprintf(b, sizeof b, "%d", v); h_text(f, x, y, b, sc, c); }
static FILE *fds[8];
static int h_fopen(const char *p, const char *m) {
    char path[400]; const char *base = strrchr(p, '/'); snprintf(path, sizeof path, "%s%s", hsdir, base ? base : p); FILE *f = fopen(path, m[0] == 'w' ? "wb" : "rb"); if (!f) return -1;
    for (int i = 0; i < 8; i++) if (!fds[i]) { fds[i] = f; return i; }
    fclose(f); return -1;
}
static int h_fread(int fd, void *b, u32 n) { return (int)fread(b, 1, n, fds[fd]); }
static int h_fwrite(int fd, const void *b, u32 n) { return (int)fwrite(b, 1, n, fds[fd]); }
static int h_fclose(int fd) { fclose(fds[fd]); fds[fd] = 0; return 0; }
static int clk_base = 13 * 3600 + 7 * 60 + 42, clk_now0 = 1000, bat_pct = 87;       /* simulated RTC: seconds since midnight at tick clk_now0 */
static void h_time(F3Time *t) { int sec = (clk_base + (int)(now - (unsigned)clk_now0) / 1000) % 86400; t->year = 2026; t->month = 10; t->day = 9; t->hour = sec / 3600; t->min = sec / 60 % 60; t->sec = sec % 60; t->wday = 5; }
static int h_battery(void) { return bat_pct; }
static F3Api api;
extern F3App *f3app_main(const F3Api *);
static F3App *app; static int alive = 1, pressed;
static void shot(const char *prefix, int n) {
    char name[300]; snprintf(name, sizeof name, "%s_%d.ppm", prefix, n); FILE *f = fopen(name, "wb"); fprintf(f, "P6\n%d %d\n255\n", F3_W, F3_H);
    for (int i = 0; i < F3_W * F3_H; i++) { u16 c = fb[i]; unsigned char rgb[3] = { (unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31) }; fwrite(rgb, 1, 3, f); }
    fclose(f); printf("shot %s\n", name);
}
static void step(int p, int x, int y) { if (!alive) return; int tp = (!p && pressed); pressed = p; now += 33; alive = app->tick(app, p, x, y, tp); }
static unsigned rs = 12345; static unsigned rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
int main(int argc, char **argv) {
    const char *prefix = argc > 1 ? argv[1] : "shot"; const char *script = argc > 2 ? argv[2] : ""; int monkey = argc > 3 ? atoi(argv[3]) : 0; int shots = 0;
    { const char *sl = strrchr(prefix, '/'); size_t n = sl ? (size_t)(sl - prefix) : 0; memcpy(hsdir, prefix, n); hsdir[n] = 0; if (!n) strcpy(hsdir, "."); }
    api.version = F3_API_VERSION; api.fb = fb; api.w = F3_W; api.h = F3_H; api.malloc = h_malloc; api.free = h_free; api.ticks = h_ticks; api.vibrate = h_vib;
    api.rect = h_rect; api.rrect = h_rrect; api.disc = h_disc; api.text = h_text; api.ctext = h_ctext; api.num = h_num; api.fopen = h_fopen; api.fread = h_fread; api.fwrite = h_fwrite; api.fclose = h_fclose; api.lang_pt = 1; api.time = h_time; api.battery = h_battery;
    if (monkey) rs = (unsigned)monkey * 2654435761u;
    app = f3app_main(&api); if (!app) { printf("f3app_main failed\n"); return 2; }
    if (monkey) {
        for (int i = 0; i < 6000 && alive; i++) {
            int k = rnd() % 10, x = rnd() % 256, y = rnd() % 402;
            if (k < 4) { step(1, x, y); step(0, x, y); }
            else if (k < 7) { int x2 = x + (int)(rnd() % 120) - 60, y2 = y + (int)(rnd() % 120) - 60; for (int s = 0; s <= 5; s++) step(1, x + (x2 - x) * s / 5, y + (y2 - y) * s / 5); step(0, x2, y2); }
            else if (k < 9) { int n = rnd() % 40; for (int s = 0; s < n; s++) step(1, x, y); step(0, x, y); }
            else { int n = rnd() % 30; for (int s = 0; s < n; s++) step(0, x, y); }
            if (i == 3000) api.lang_pt = 0;
        }
        printf("monkey seed %d: alive=%d oob=%d draws=%d vib=%d\n", monkey, alive, oob, calls, vib); if (alive && app->close) app->close(app); return oob ? 1 : 0;
    }
    for (const char *p = script; *p;) {
        while (*p == ' ') p++; if (!*p) break; int a, b, c2, d, n;
        if (!strncmp(p, "shot", 4)) { shot(prefix, shots++); p += 4; }
        else if (!strncmp(p, "lang0", 5)) { api.lang_pt = 0; p += 5; } else if (!strncmp(p, "lang1", 5)) { api.lang_pt = 1; p += 5; }
        else if (!strncmp(p, "exp", 3)) { n = atoi(p + 3); printf("%s vib>=%d (have %d)\n", vib >= n ? "ok  " : "FAIL", n, vib); if (vib < n) return 1; while (*p && *p != ' ') p++; }
        else if (*p == 'c') { int hh, mm, ss; sscanf(p + 1, "%d:%d:%d", &hh, &mm, &ss); clk_base = hh * 3600 + mm * 60 + ss; clk_now0 = (int)now; while (*p && *p != ' ') p++; }
        else if (*p == 'b') { bat_pct = atoi(p + 1); while (*p && *p != ' ') p++; }
        else if (*p == 't') { n = atoi(p + 1); for (int i = 0; i < n; i++) step(0, 0, 0); while (*p && *p != ' ') p++; }
        else if (*p == 'p') { sscanf(p + 1, "%d,%d", &a, &b); step(1, a, b); step(0, a, b); while (*p && *p != ' ') p++; }
        else if (*p == 'h') { sscanf(p + 1, "%d,%d:%d", &a, &b, &n); for (int i = 0; i < n; i++) step(1, a, b); step(0, a, b); while (*p && *p != ' ') p++; }
        else if (*p == 's') { sscanf(p + 1, "%d,%d>%d,%d", &a, &b, &c2, &d); for (int s = 0; s <= 5; s++) step(1, a + (c2 - a) * s / 5, b + (d - b) * s / 5); step(0, c2, d); while (*p && *p != ' ') p++; }
        else { printf("bad token: %s\n", p); return 3; }
    }
    printf("end: alive=%d oob=%d draws=%d vib=%d\n", alive, oob, calls, vib); if (alive && app->close) app->close(app); return oob ? 1 : 0;
}
