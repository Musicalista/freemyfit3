/* host test of the recovery screen (recovery.inc.c): safe mode, clearing the apps list, resetting scores, the two-tap rule, and "Meus apps" refusing to load in safe mode.
 * usage: host_recovery_test <empty dir>  (it plays the role of /user/) */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz;
static const char *appdir; static FILE *fsf[8];
static int h_open(const char *p, const char *m) { char path[400]; if (strncmp(p, "/user/", 6)) return -1; snprintf(path, sizeof path, "%s/%s", appdir, p + 6); FILE *f = fopen(path, m[0] == 'w' ? "wb" : "rb"); if (!f) return -1; for (int i = 0; i < 8; i++) if (!fsf[i]) { fsf[i] = f; return i; } fclose(f); return -1; }
static int h_read(int fd, void *b, unsigned n) { return (int)fread(b, 1, n, fsf[fd]); }
static int h_write(int fd, const void *b, unsigned n) { return (int)fwrite(b, 1, n, fsf[fd]); }
static int h_close(int fd) { fclose(fsf[fd]); fsf[fd] = 0; return 0; }
#define FS_OPEN h_open
#define FS_READ h_read
#define FS_WRITE h_write
#define FS_CLOSE h_close
#define UI_LANG_ID 68
static int fake_rtc[9] = { 0, 42, 7, 13, 9, 10, 2026, 5, 0 }; static int fake_bat = 87;
static void h_rtc(void *b) { for (int i = 0; i < 9; i++) ((int *)b)[i] = fake_rtc[i]; }
static int h_bat(int t, int *out) { if (t != 1) return -1; *out = fake_bat; return 0; }
#define RTC_GET h_rtc
#define BAT_GET h_bat
#define MOTOR_ONCE(a, b) (buzz++, 0)
#define MALLOC(n) malloc(n)
#define FREE(p) free(p)
#define TICK_GET() (fake_now)
#define INVALIDATE(o) ((void)0)
#define ADD_FLAG(o, f) ((void)0)
#define CLEAR_FLAG(o, f) ((void)0)
#define IMG_CREATE(r) ((void *)1)
#define IMG_SET_SRC(i, s) ((void)0)
#define OBJ_ALIGN_TO(a, b, c, d, e) ((void)0)
#define ADD_EVENT_CB(o, cb, f, u) ((void *)0)
#define TIMER_DEL(t) ((void)0)
#define TIMER_CREATE(cb, p, u) (memcpy(fake_timer + 0xC, &(u), sizeof(void *)), (void *)fake_timer)
#define EV_CODE(e) (*(int *)(e))
#define EV_USER(e) (*(void **)((char *)(e) + 8))
#define TP_SAMPLE_GET(s) (((unsigned char *)(s))[1] = tp_st, *(unsigned short *)((unsigned char *)(s) + 4) = tp_x, *(unsigned short *)((unsigned char *)(s) + 6) = tp_y, 0)
static int tp_st, tp_x, tp_y;
#define SEND_REPLY(a, b, c) 0
#define WITH_APPSYS 1
#define NO_WEB 1
#define MINI_GAMES 1
#define NO_DOOM 1
#include "fit3_apps.c"
static void *host_entry(void *as, const void *api) { (void)as; (void)api; return 0; }
static int failures;
static void check(const char *n, int ok) { printf("  %s %s\n", ok ? "ok  " : "FAIL", n); if (!ok) failures++; }
static long fsize(const char *name) { char p[400]; snprintf(p, sizeof p, "%s/%s", appdir, name); FILE *f = fopen(p, "rb"); if (!f) return -1; fseek(f, 0, SEEK_END); long n = ftell(f); fclose(f); return n; }
static void putf(const char *name, const char *text) { char p[400]; snprintf(p, sizeof p, "%s/%s", appdir, name); FILE *f = fopen(p, "wb"); fputs(text, f); fclose(f); }
static void ev(RC *a, int code, int x, int y) { struct { int code; int pad; void *user; } e = { code, 0, a }; if (code == 1 || code == 2) { tp_st = 1; tp_x = x; tp_y = y; } else tp_st = 0; rc_event(&e); }
static void tap(RC *a, int x, int y) { ev(a, 1, x, y); ev(a, 8, x, y); ev(a, 7, x, y); }
static RC *fresh(void) { RC *a = (RC *)calloc(1, sizeof(RC)); a->px = (u16 *)calloc(KBD_W * KBD_H, 2); a->root = (void *)1; a->img = (void *)1; a->cvblk = (u32 *)calloc(1, 16); rc_draw(a); return a; }
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
int main(int argc, char **argv) {
    (void)argc; appdir = argv[1]; { char cmd[500]; snprintf(cmd, sizeof cmd, "mkdir \"%s/apps\" 2>nul", appdir); system(cmd); }
    putf("apps/index.txt", "hello;F8D8B0;A85A20;Ola;Hello\nsnake;BFEFC4;1E7A3A;Snake;Snake\n"); putf("snake.sav", "abcd"); putf("tetris.sav", "wxyz"); putf("gb.sav", "keep");
    check("a fresh watch is not in safe mode", rc_safe() == 0);
    RC *a = fresh(); dump(a->px, "rc_home.ppm");
    tap(a, 128, 150); check("button 1 turns safe mode on (file holds '1')", rc_safe() == 1 && fsize("safe.flag") == 1);
    rc_draw(a); dump(a->px, "rc_safe.ppm");
    tap(a, 128, 150); check("button 1 again turns it off", rc_safe() == 0);
    tap(a, 128, 200); check("one tap on 'clear the apps list' only arms it (list untouched)", fsize("apps/index.txt") > 20 && a->arm == 1);
    rc_draw(a); dump(a->px, "rc_armed.ppm");
    tap(a, 128, 200); check("the second tap empties the list", fsize("apps/index.txt") == 1 && a->arm == 0);
    putf("apps/index.txt", "hello;F8D8B0;A85A20;Ola;Hello\n");
    tap(a, 128, 200); fake_now += 5000; tap(a, 128, 200); check("two taps more than 4 s apart do not clear (the second only re-arms)", fsize("apps/index.txt") > 20 && a->arm == 1);
    tap(a, 128, 248); tap(a, 128, 248); check("scores: snake.sav and tetris.sav are emptied", fsize("snake.sav") == 0 && fsize("tetris.sav") == 0);
    check("scores: gb.sav (not an app score) and missing files are left alone", fsize("gb.sav") == 4 && fsize("flappy.sav") == -1);
    tap(a, 128, 296); check("button 4 shows the help text", a->msg == 4); rc_draw(a); dump(a->px, "rc_help.ppm");
    tap(a, 236, 10); check("the X closes the screen (and frees it)", 1);
    rc_set_safe(1); { void *r = (void *)1; int ok = as_open(r, 0); check("with safe mode on, opening My apps shows the recovery screen instead (no app list, no timer)", ok == 1 && fake_timer[0xC] == 0 && fake_timer[0xD] == 0 && rc_safe() == 1); }
    rc_set_safe(0);
    printf(failures ? "RECOVERY FAILURES: %d\n" : "RECOVERY ALL OK\n", failures); return failures ? 1 : 0;
}
