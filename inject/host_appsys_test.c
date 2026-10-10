/* host test of the app loader ("Meus apps"): real .f3a files (ARM images built by apps/appcc.py) are parsed, CRC-checked and relocated; execution is replaced by the same sources compiled natively.
 * usage: host_appsys_test <dir with index.txt and *.f3a>  (the files are mapped to /user/apps/) */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz;
static const char *appdir; static FILE *fsf[8];
static int h_open(const char *p, const char *m) { char path[400]; if (strncmp(p, "/user/apps/", 11)) return -1; snprintf(path, sizeof path, "%s/%s", appdir, p + 11); FILE *f = fopen(path, m[0] == 'r' ? "rb" : "wb"); if (!f) return -1; fsf[3] = f; return 3; }
static int h_read(int fd, void *b, unsigned n) { return (int)fread(b, 1, n, fsf[fd]); }
static int h_write(int fd, const void *b, unsigned n) { return (int)fwrite(b, 1, n, fsf[fd]); }
static int h_close(int fd) { fclose(fsf[fd]); return 0; }
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
static void *host_entry(void *as, const void *api);
#define AS_CALL(a, fn) ((F3App *)host_entry((void *)(a), (const void *)&(a)->api))
#include "fit3_apps.c"
extern F3App *hello_main(const F3Api *), *paint_main(const F3Api *), *dice_main(const F3Api *);
static int relocs_checked;
static void *host_entry(void *as, const void *api) {
    AS *a = (AS *)as; const char *id = a->id[a->cur]; relocs_checked++;
    if (!strcmp(id, "hello")) return hello_main((const F3Api *)api); if (!strcmp(id, "paint")) return paint_main((const F3Api *)api); if (!strcmp(id, "dice")) return dice_main((const F3Api *)api); return 0;
}
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static int failures;
static void check(const char *n, int ok) { printf("  %s %s\n", ok ? "ok  " : "FAIL", n); if (!ok) failures++; }
static void ev(AS *a, int code, int x, int y) { struct { int code; int pad; void *user; } e = { code, 0, a }; if (code == 1 || code == 2) { tp_st = 1; tp_x = x; tp_y = y; } else tp_st = 0; as_event(&e); }
static void tap(AS *a, int x, int y) { ev(a, 1, x, y); ev(a, 8, x, y); a->x = x; a->y = y; ev(a, 7, x, y); }
static void frames(AS *a, int n) { for (int i = 0; i < n; i++) { fake_now += 33; as_tick(fake_timer); } }
int main(int argc, char **argv) {
    appdir = argv[1];
    as_open((void *)1, 0); AS *a = *(AS **)(fake_timer + 0xC); if (!a) { puts("as_open failed"); return 1; }
    printf("apps in the index: %d\n", a->napps); check("index parsed (3 apps)", a->napps == 3); for (int i = 0; i < a->napps; i++) printf("   %s | %s | %s\n", a->id[i], a->npt[i], a->nen[i]);
    dump(a->px, "as_list.ppm");
    tap(a, 60, 90); frames(a, 2); check("tapping the first tile loads and starts the app (CRC + relocations accepted)", a->running && a->app && a->err == 0 && relocs_checked == 1);
    ev(a, 1, 200, 200); frames(a, 3); tap(a, 200, 200); frames(a, 3); dump(a->px, "as_hello.ppm");
    tap(a, 236, 10); check("X stops the app and returns to the list", !a->running && !a->app);
    tap(a, 180, 90); frames(a, 2); check("second app (paint) runs", a->running);
    ev(a, 1, 60, 100); for (int i = 0; i < 20; i++) { ev(a, 2, 60 + i * 8, 100 + i * 6); frames(a, 1); } ev(a, 8, 220, 220); tap(a, 6 + 26 * 4 + 10, 380); frames(a, 2);
    ev(a, 1, 40, 250); for (int i = 0; i < 20; i++) { ev(a, 2, 40 + i * 8, 250 - i * 4); frames(a, 1); } ev(a, 8, 200, 170); frames(a, 2); dump(a->px, "as_paint.ppm");
    tap(a, 236, 10); tap(a, 60, 190); frames(a, 2); check("third app (dice) runs", a->running); tap(a, 128, 250); frames(a, 30); dump(a->px, "as_dice.ppm"); tap(a, 236, 10);
    { F3Time t; a->api.time(&t); check("api.time reads the RTC layout (2026-10-09 13:07:42, wday 5)", t.year == 2026 && t.month == 10 && t.day == 9 && t.hour == 13 && t.min == 7 && t.sec == 42 && t.wday == 5);
      check("api.battery returns the percent (87)", a->api.battery() == 87); fake_bat = 250; check("api.battery clamps to 100", a->api.battery() == 100); fake_bat = 87; }
    /* corrupt files must be refused */
    { char p[400]; unsigned char buf[2000]; snprintf(p, sizeof p, "%s/hello.f3a", appdir); FILE *f = fopen(p, "rb"); size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
      buf[100] ^= 1; f = fopen(p, "wb"); fwrite(buf, 1, n, f); fclose(f); a->err = as_load(a, 0); check("bit flip in the image -> refused (error 2)", a->err == 2 && !a->app);
      buf[100] ^= 1; buf[0] = 'X'; f = fopen(p, "wb"); fwrite(buf, 1, n, f); fclose(f); a->err = as_load(a, 0); check("bad magic -> refused (error 2)", a->err == 2);
      buf[0] = 'F'; f = fopen(p, "wb"); fwrite(buf, 1, n - 40, f); fclose(f); a->err = as_load(a, 0); check("truncated file -> refused (error 2)", a->err == 2);
      f = fopen(p, "wb"); fwrite(buf, 1, n, f); fclose(f); a->err = as_load(a, 0); check("restored file loads again", a->err == 0); as_stop(a);
      remove(p); a->err = as_load(a, 0); check("missing file -> error 1", a->err == 1); }
    printf(failures ? "APPSYS FAILURES: %d\n" : "APPSYS ALL OK\n", failures); return failures ? 1 : 0;
}
