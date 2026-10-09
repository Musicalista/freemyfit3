/* host test of the Game Boy front end: the real fit3_apps.c with LVGL faked and /user/gb.gb mapped to a local file. usage: host_gb_ui_test rom out.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz;
static FILE *fsf[8]; static const char *rompath;
static int h_open(const char *p, const char *m) { FILE *f = fopen("gm2_unused.txt", m[0] == (char)114 ? "rb" : "wb"); if (!f) { puts(p); return -1; } fsf[3] = f; return 3; }
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
#define WITH_GAMES2 1
#define NO_WEB 1
#define MINI_GAMES 1
#define NO_DOOM 1
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static void ev(GM2 *t, int code, int x, int y) {
    struct { int code; int pad; void *user; } e = { code, 0, t };
    if (code == 1 || code == 2) { tp_st = 1; tp_x = x; tp_y = y; } else tp_st = 0;
    gm2_event(&e);
}
static void tap(GM2 *t, int x, int y) { ev(t, 1, x, y); ev(t, 8, x, y); t->x = x; t->y = y; ev(t, 7, x, y); }
static void frames(GM2 *t, int n, int ms) { for (int i = 0; i < n; i++) { fake_now += (unsigned)ms; gm2_tick(fake_timer); } }
int main(void) {
    gm2_open((void *)1, 0); GM2 *t = *(GM2 **)(fake_timer + 0xC); if (!t) { puts("gm2_open failed"); return 1; }
    dump(t->px, "gm2_grid.ppm");
    /* Pong: the player's paddle follows a finger placed under the ball: should beat a bored CPU sometimes; just check the rules */
    tap(t, 50, 70); printf("pong kind=%d state=%d\n", t->kind, t->pstate); tap(t, 100, 200); printf("served: state=%d\n", t->pstate);
    int guard = 0; ev(t, 1, 100, 390); while (t->pstate != 2 && guard++ < 60000) { if (t->pstate == 0) { tap(t, 100, 200); ev(t, 1, 100, 390); } tp_st = 1; tp_x = (int)t->bx + 4 + ((t->hits & 1) ? 22 : -22); ev(t, 2, tp_x, 390); frames(t, 1, 33); }
    printf("pong over: player %d - cpu %d (state %d) after %d frames\n", t->ps, t->as, t->pstate, guard); ev(t, 8, 100, 390); dump(t->px, "gm2_pong.ppm");
    tap(t, 128, 190); printf("restart: scores %d-%d state %d\n", t->ps, t->as, t->pstate);
    tap(t, 236, 10); printf("X -> kind=%d\n", t->kind);
    /* tic-tac-toe: play random moves against the CPU many times; the CPU must never lose when it does not slip */
    tap(t, 190, 70); printf("ttt kind=%d\n", t->kind);
    for (int game = 0; game < 300; game++) { int guard2 = 0; while (!t->tover && guard2++ < 30) { int cell = -1; for (int k = 0; k < 9 && cell < 0; k++) { int c = (int)(gm2_rand(t) % 9u); if (!t->tb[c]) cell = c; } if (cell < 0) for (int k = 0; k < 9; k++) if (!t->tb[k]) cell = k; if (cell < 0) break; tap(t, 18 + (cell % 3) * 74 + 30, 50 + (cell / 3) * 74 + 30); } if (t->tover) tap(t, 128, 300); }
    printf("ttt 300 games vs random: X wins %d, draws %d, O wins %d\n", t->tsx, t->tsd, t->tso);
    tap(t, 18 + 30, 50 + 30); frames(t, 1, 33); dump(t->px, "gm2_ttt.ppm"); tap(t, 236, 10);
    /* sudoku */
    tap(t, 50, 170); printf("sudoku kind=%d\n", t->kind);
    { int ok = 1; for (int r = 0; r < 9; r++) { int seen = 0, seenc = 0; for (int c = 0; c < 9; c++) { seen |= 1 << t->sol[r * 9 + c]; seenc |= 1 << t->sol[c * 9 + r]; } if (seen != 0x3FE || seenc != 0x3FE) ok = 0; }
      for (int b = 0; b < 9; b++) { int seen = 0; for (int k = 0; k < 9; k++) seen |= 1 << t->sol[(b / 3 * 3 + k / 3) * 9 + b % 3 * 3 + k % 3]; if (seen != 0x3FE) ok = 0; }
      int clues = 0; for (int i = 0; i < 81; i++) if (t->puz[i]) clues++; printf("solution valid=%d clues=%d\n", ok, clues); }
    for (int i = 0; i < 81 && !t->sdone; i++) { if (t->puz[i]) continue; tap(t, SD_X + (i % 9) * SD_C + 10, SD_Y + (i / 9) * SD_C + 10); tap(t, 6 + (t->sol[i] - 1) * 27 + 5, 300); }
    printf("filled with the solution: done=%d conflicts=%d\n", t->sdone, t->sconf);
    tap(t, 172 + 10, 340); printf("new level=%d done=%d\n", t->slevel, t->sdone);
    { int i = 0; while (t->puz[i]) i++; tap(t, SD_X + (i % 9) * SD_C + 10, SD_Y + (i / 9) * SD_C + 10); int wrong = t->sol[i] % 9 + 1; tap(t, 6 + (wrong - 1) * 27 + 5, 300); printf("wrong digit placed: cell has %d (solution %d) conflict-or-not=%d\n", t->cur[i], t->sol[i], sd_conflict(t->cur, i));
      tap(t, 90 + 10, 340); printf("hint: cell now %d (solution %d), hints=%d\n", t->cur[i], t->sol[i], t->shints); }
    frames(t, 1, 33); dump(t->px, "gm2_sudoku.ppm"); tap(t, 236, 10);
    /* memory */
    tap(t, 190, 170); printf("memory kind=%d\n", t->kind);
    { int guess = 0; while (!t->mdone && guess++ < 200) {
        int a = -1, b = -1; for (int i = 0; i < 16 && b < 0; i++) { if (t->mo[i]) continue; if (a < 0) a = i; else if (t->mc[i] == t->mc[a]) b = i; }
        tap(t, 3 + (a % 4) * 64 + 20, 40 + (a / 4) * 78 + 20); tap(t, 3 + (b % 4) * 64 + 20, 40 + (b / 4) * 78 + 20); frames(t, 2, 33); }
      printf("memory solved perfectly in %d moves (done=%d)\n", t->mmoves, t->mdone); }
    dump(t->px, "gm2_mem.ppm");
    tap(t, 128, 190); { int a = 0, b = 1; while (t->mc[a] == t->mc[b]) b++; tap(t, 3 + (a % 4) * 64 + 20, 40 + (a / 4) * 78 + 20); tap(t, 3 + (b % 4) * 64 + 20, 40 + (b / 4) * 78 + 20); printf("mismatch: open=%d,%d\n", t->mo[a], t->mo[b]); frames(t, 1, 900); frames(t, 1, 33); printf("after 0.9 s: open=%d,%d\n", t->mo[a], t->mo[b]); }
    return 0;
}
