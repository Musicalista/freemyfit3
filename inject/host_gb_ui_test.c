/* host test of the Game Boy front end: the real fit3_apps.c with LVGL faked and /user/gb.gb mapped to a local file. usage: host_gb_ui_test rom out.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz;
static FILE *fsf[8]; static const char *rompath;
static int h_open(const char *p, const char *m) { FILE *f = fopen(strstr(p, ".sav") ? "test.sav" : rompath, m[0] == (char)114 ? "rb" : "wb"); if (!f) { puts(p); return -1; } fsf[3] = f; return 3; }
static int h_read(int fd, void *b, unsigned n) { return (int)fread(b, 1, n, fsf[fd]); }
static int h_write(int fd, const void *b, unsigned n) { return (int)fwrite(b, 1, n, fsf[fd]); }
static int h_close(int fd) { fclose(fsf[fd]); return 0; }
#define FS_OPEN h_open
#define FS_READ h_read
#define FS_WRITE h_write
#define FS_CLOSE h_close
#define UI_LANG_ID 30
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
#define WITH_GB 1
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
int main(int argc, char **argv) {
    rompath = argv[1];
    gb_open((void *)1, 0); GBA *a = *(GBA **)(fake_timer + 0xC);
    if (!a) { puts("gb_open: load failed"); return 1; }
    struct { int code; int pad; void *user; } ev = { 1, 0, a };
    for (int i = 0; i < atoi(argv[3]); i++) {
        if (i == 120) { tp_st = 1; tp_x = 222; tp_y = 312; ev.code = 1; gb_event(&ev); }     /* press A */
        if (i == 130) { tp_st = 0; ev.code = 8; gb_event(&ev); }
        fake_now += 17; gb_tick(fake_timer);
    }
    printf("frames done=%u lock=%d/%d\n", a->done, a->lockA, a->lockB);
    dump(a->px, argv[2]); return 0;
}
