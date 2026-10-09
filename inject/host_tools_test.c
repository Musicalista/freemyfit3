/* host test of the Game Boy front end: the real fit3_apps.c with LVGL faked and /user/gb.gb mapped to a local file. usage: host_gb_ui_test rom out.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz;
static FILE *fsf[8]; static const char *rompath;
static int h_open(const char *p, const char *m) { FILE *f = fopen("tools_notes.txt", m[0] == (char)114 ? "rb" : "wb"); if (!f) { puts(p); return -1; } fsf[3] = f; return 3; }
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
#define WITH_TOOLS 1
#define NO_WEB 1
#define MINI_GAMES 1
#define NO_DOOM 1
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static void ev(TL *t, int code, int x, int y) {
    struct { int code; int pad; void *user; } e = { code, 0, t };
    if (code == 1 || code == 2) { tp_st = 1; tp_x = x; tp_y = y; } else tp_st = 0;
    tl_event(&e);
}
static void tap(TL *t, int x, int y) { ev(t, 1, x, y); ev(t, 8, x, y); t->x = x; t->y = y; ev(t, 7, x, y); }
static void frames(TL *t, int n, int ms) { for (int i = 0; i < n; i++) { fake_now += (unsigned)ms; tl_tick(fake_timer); } }
static void key(TL *t, char k) { for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) if (tl_ckeys[r * 4 + c] == k) tap(t, 6 + c * 62 + 10, 106 + r * 58 + 10); }
int main(void) {
    remove("tools_notes.txt");
    tl_open((void *)1, 0); TL *t = *(TL **)(fake_timer + 0xC); if (!t) { puts("tl_open failed"); return 1; }
    dump(t->px, "tl_grid.ppm");
    tap(t, 50, 70); printf("calculator: kind=%d\n", t->kind); frames(t, 1, 33);
    key(t, '1'); key(t, '2'); key(t, '+'); key(t, '3'); key(t, '0'); key(t, '='); printf("12+30 = %s\n", t->entry);
    key(t, '*'); key(t, '2'); key(t, '='); printf("x2 = %s\n", t->entry);
    key(t, 'C'); key(t, '1'); key(t, '/'); key(t, '3'); key(t, '='); printf("1/3 = %s\n", t->entry);
    key(t, 'C'); key(t, '5'); key(t, '/'); key(t, '0'); key(t, '='); printf("5/0 = %s (err=%d)\n", t->entry, t->err);
    key(t, 'C'); key(t, '9'); key(t, '.'); key(t, '5'); key(t, '-'); key(t, '2'); key(t, '.'); key(t, '2'); key(t, '5'); key(t, '='); printf("9.5-2.25 = %s\n", t->entry);
    key(t, 'C'); key(t, '5'); key(t, 'S'); key(t, '*'); key(t, '4'); key(t, '='); printf("-5*4 = %s\n", t->entry);
    key(t, 'C'); key(t, '8'); key(t, '0'); key(t, '%'); printf("80%% = %s\n", t->entry);
    key(t, 'C'); key(t, '1'); key(t, '2'); key(t, '3'); key(t, '<'); printf("123<- = %s\n", t->entry);
    key(t, 'C'); key(t, '0'); key(t, '.'); key(t, '1'); key(t, '+'); key(t, '0'); key(t, '.'); key(t, '2'); key(t, '='); printf("0.1+0.2 = %s\n", t->entry);
    key(t, 'C'); key(t, '9'); key(t, '9'); key(t, '9'); key(t, '9'); key(t, '9'); key(t, '9'); key(t, '9'); key(t, '9'); key(t, '9'); key(t, '*'); key(t, '9'); key(t, '='); printf("999999999*9 = %s\n", t->entry);
    key(t, 'C'); key(t, '7'); key(t, '8'); key(t, '+'); key(t, '4'); frames(t, 1, 33); dump(t->px, "tl_calc.ppm");
    tap(t, 236, 10); printf("X -> kind=%d (grid=0)\n", t->kind);
    /* stopwatch */
    tap(t, 130, 70 + 0); printf("tile 1: kind=%d\n", t->kind); frames(t, 1, 33);
    tap(t, 60, 175); frames(t, 30, 100); tap(t, 190, 175); frames(t, 20, 100); tap(t, 190, 175); printf("running=%d laps=%d lap1=%u lap2=%u ms\n", t->run, t->nlaps, t->laps[0], t->laps[1]);
    tap(t, 60, 175); printf("stopped at %u ms\n", t->base); frames(t, 1, 33); dump(t->px, "tl_chrono.ppm");
    tap(t, 190, 175); printf("reset: base=%u laps=%d\n", t->base, t->nlaps);
    /* timer */
    tap(t, 190, 49); printf("timer tab=%d set=%u ms\n", t->tab, t->tset); tap(t, 126, 160); tap(t, 206, 160); printf("+1m +10s: set=%u\n", t->tset);
    tap(t, 60, 224); frames(t, 10, 100); printf("running=%d left=%u ms\n", t->trun, tl_tm_ms(t));
    fake_now += 400000; frames(t, 3, 50); printf("after the time passed: running=%d ring=%d buzz=%d\n", t->trun, t->tring, buzz); frames(t, 5, 1100); printf("ring now %d, buzz=%d\n", t->tring, buzz);
    tap(t, 190, 224); frames(t, 1, 33); dump(t->px, "tl_timer.ppm");
    tap(t, 236, 10);
    /* flashlight */
    tap(t, 50, 200); printf("flashlight kind=%d col=%d\n", t->kind, t->lcol); tap(t, 100, 200); tap(t, 100, 200); printf("col=%d\n", t->lcol); frames(t, 1, 33); dump(t->px, "tl_light.ppm"); tap(t, 236, 10);
    /* notes */
    tap(t, 190, 200); printf("notes kind=%d lines=%d\n", t->kind, tl_nlines(t));
    tl_note_done(t, "Comprar leite e pao"); tl_note_done(t, "Ligar para a Ana"); tl_note_done(t, "Reuniao 15h sala 3"); frames(t, 1, 33); printf("lines=%d sel=%d len=%d\n", tl_nlines(t), t->nsel, t->nlen);
    tap(t, 30, 323); printf("up: sel=%d\n", t->nsel); frames(t, 1, 33); dump(t->px, "tl_notes.ppm"); tap(t, 220, 323); printf("deleted: lines=%d\n", tl_nlines(t));
    tap(t, 236, 10); tap(t, 190, 200); printf("reopen notes: lines=%d (persisted in file)\n", tl_nlines(t)); tap(t, 236, 10);
    /* counter */
    tap(t, 50, 300); printf("counter kind=%d\n", t->kind); tap(t, 100, 230); tap(t, 100, 230); tap(t, 100, 230); tap(t, 100, 310); printf("count=%d\n", t->cnt); frames(t, 1, 33); dump(t->px, "tl_count.ppm"); tap(t, 100, 370); printf("reset=%d\n", t->cnt); tap(t, 236, 10);
    /* dice */
    tap(t, 190, 300); printf("dice kind=%d\n", t->kind); { int mn = 99, mx = 0; for (int i = 0; i < 200; i++) { tap(t, 100, 300); if (t->dres < mn) mn = t->dres; if (t->dres > mx) mx = t->dres; } printf("d6 range %d..%d\n", mn, mx); }
    tap(t, 8 + 62 * 2 + 10, 50); { int mn = 99, mx = 0; for (int i = 0; i < 400; i++) { tap(t, 100, 300); if (t->dres < mn) mn = t->dres; if (t->dres > mx) mx = t->dres; } printf("d20 range %d..%d\n", mn, mx); }
    tap(t, 8 + 62 * 1 + 10, 50); tap(t, 100, 300); printf("2d6 = %d (%d)\n", t->dres, t->dres2); frames(t, 1, 33); dump(t->px, "tl_dice.ppm");
    tap(t, 236, 10); tap(t, 236, 10); printf("closed: from_menu=0 so nothing else; kind ok\n");
    return 0;
}
