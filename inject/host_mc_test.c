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
#define NO_DOOM 1
#define NO_WEB 1
#define MINI_GAMES 1
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static void ev(GSt *st, int code, int x, int y) {
    struct { int code; int pad; void *user; } e = { code, 0, st };
    if (code == 1 || code == 2) { tp_st = 1; tp_x = x; tp_y = y; } else tp_st = 0;
    game_event(&e);
}
static void tap(GSt *st, int x, int y) { ev(st, 1, x, y); ev(st, 8, x, y); st->x = x; st->y = y; ev(st, 7, x, y); }
static void frames(GSt *st, int n) { for (int i = 0; i < n; i++) { fake_now += 33; game_tick(fake_timer); } }
int main(int argc, char **argv) {
    rompath = "mc_test.sav";
    game_open((void *)1, 0); GSt *st = *(GSt **)(fake_timer + 0xC); if (!st) { puts("game_open failed"); return 1; }
    printf("screen=%d has_save=%d\n", st->screen, st->has_save);
    dump(st->cv, "mc_menu.ppm");
    tap(st, 100, 230); printf("type -> %d\n", st->wtype); tap(st, 100, 290); frames(st, 2); dump(st->cv, "mc_menu2.ppm");
    tap(st, 100, 190); printf("new world: screen=%d seed=%u type=%d\n", st->screen, st->seed, st->wtype);
    frames(st, 30); dump(st->cv, "mc_play.ppm");
    { Vox *g = st->g; g->pitch = 35; g->yaw = 20; st->pressed = 0; frames(st, 5);
      printf("hit=%d at %d,%d,%d\n", g->has_hit, g->hit.x, g->hit.y, g->hit.z); int bx = g->hit.x, by = g->hit.y, bz = g->hit.z, b0 = vx_get(g, bx, by, bz);
      tap(st, 128, 130); printf("tap break: %d -> %d modified=%d\n", b0, vx_get(g, bx, by, bz), g->modified);
      ev(st, 1, 128, 130); st->x = 128; st->y = 130; ev(st, 5, 128, 130); ev(st, 8, 128, 130); printf("long press place: %d (sel %d hot %d)\n", vx_get(g, bx, by, bz), g->sel, g->hot[g->sel]);
      tap(st, 2 + 28 * 3 + 5, 380); printf("select slot -> %d\n", g->sel);
      tap(st, 185, 285); printf("fly toggled: %d\n", g->flying); tap(st, 185, 285); printf("fly toggled: %d\n", g->flying);
      /* walk forward by holding the ^ button */
      ev(st, 1, 80, 285); float z0 = g->z, x0 = g->x; frames(st, 60); ev(st, 8, 80, 285); printf("walked: dx=%.2f dz=%.2f\n", g->x - x0, g->z - z0);
      /* drag to look */
      float yw = g->yaw; ev(st, 1, 100, 100); st->sx = 100; ev(st, 2, 140, 100); frames(st, 2); ev(st, 8, 140, 100); printf("drag: yaw %.1f -> %.1f\n", yw, g->yaw);
      frames(st, 20); dump(st->cv, "mc_play2.ppm");
      tap(st, 232, 285); printf("inventory: screen=%d\n", st->screen); frames(st, 2); dump(st->cv, "mc_inv.ppm");
      tap(st, 8 + 48 * 2 + 20, 62 + 64 + 20); printf("picked item -> hot[%d]=%d (%s)\n", st->g->sel, st->g->hot[st->g->sel], bl_en[st->g->hot[st->g->sel]]);
      frames(st, 2); dump(st->cv, "mc_inv2.ppm"); tap(st, 100, 320); printf("back: screen=%d\n", st->screen);
      tap(st, 236, 10); printf("pause: screen=%d\n", st->screen); frames(st, 2); dump(st->cv, "mc_pause.ppm");
      tap(st, 100, 180); printf("save: msg=%d\n", st->msg);
      tap(st, 100, 230); printf("main menu: screen=%d has_save=%d\n", st->screen, st->has_save); frames(st, 2);
      float px = g->x; int blk = vx_get(g, bx, by, bz);
      tap(st, 100, 140); printf("continue: screen=%d x %.2f vs %.2f block %d vs %d\n", st->screen, st->g->x, px, vx_get(st->g, bx, by, bz), blk);
    }
    return 0;
}
