#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static int tp_st, tp_x, tp_y;
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz;
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
#define EV_TARGET(e) ((void *)1)
#define touch_read_stub 1
#define UI_LANG_ID 68
#define TP_SAMPLE_GET(s) (((unsigned char *)(s))[1] = tp_st, *(unsigned short *)((unsigned char *)(s) + 4) = tp_x, *(unsigned short *)((unsigned char *)(s) + 6) = tp_y, 0)
#define SEND_REPLY(a, b, c) 0
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static void show(G2048 *g) { for (int r = 0; r < 4; r++) { for (int c = 0; c < 4; c++) printf("%4d", g->b[r][c] ? 1 << g->b[r][c] : 0); printf("\n"); } }
int main(void) {
    mg_open((void *)1, MG_2048, 0); MG *m = *(MG **)(fake_timer + 0xC); G2048 *g = &m->g.g2;
    /* merge rules: [2 2 4 4] left -> [4 8 0 0] ; [2 2 2 2] left -> [4 4 0 0] ; [2 0 2 4] left -> [4 4 0 0] */
    memset(g->b, 0, sizeof g->b); g->score = 0;
    g->b[0][0] = 1; g->b[0][1] = 1; g->b[0][2] = 2; g->b[0][3] = 2;
    g->b[1][0] = 1; g->b[1][1] = 1; g->b[1][2] = 1; g->b[1][3] = 1;
    g->b[2][0] = 1; g->b[2][1] = 0; g->b[2][2] = 1; g->b[2][3] = 2;
    g2_move(m, 3); show(g);
    printf("score=%d (expect 4+8 +4+4 +4 = 24)\n", g->score);
    /* random play until game over */
    mg_open((void *)1, MG_2048, 0); m = *(MG **)(fake_timer + 0xC); g = &m->g.g2; int moves = 0;
    for (int i = 0; i < 20000 && !g->over; i++) { int d = (int)(mg_rand(m) % 4u); m->gev = d + 1; fake_now += 33; mg_tick(fake_timer); moves++; }
    printf("random play: over=%d score=%d moves=%d best tile=", g->over, g->score, moves);
    int best = 0; for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (g->b[r][c] > best) best = g->b[r][c]; printf("%d\n", 1 << best);
    dump(m->px, "g2048_over.ppm");
    /* a mid-game board for the screenshot */
    mg_open((void *)1, MG_2048, 0); m = *(MG **)(fake_timer + 0xC); g = &m->g.g2;
    for (int i = 0; i < 60; i++) { m->gev = (i % 3 == 0) ? 4 : (i % 3 == 1) ? 3 : 2; fake_now += 33; mg_tick(fake_timer); }
    g->b[3][3] = 11; g->b[3][2] = 10; g->b[3][1] = 9; mg_draw(m); dump(m->px, "g2048.ppm"); printf("score=%d\n", g->score);
    /* swipe detection through the real event handler */
    mg_open((void *)1, MG_2048, 0); m = *(MG **)(fake_timer + 0xC);
    { struct { int code; int pad; void *user; } ev = { 1, 0, m }; tp_st = 1; tp_x = 100; tp_y = 200; mg_event(&ev);
      ev.code = 2; tp_x = 108; mg_event(&ev); printf("drag 8px -> gev=%d (0 = none yet)\n", m->gev);
      tp_x = 130; mg_event(&ev); printf("drag 30px right -> gev=%d (2 = right, recognised while dragging)\n", m->gev);
      ev.code = 7; mg_event(&ev); printf("click after a drag -> tap=%d (0 = ignored)\n", m->tap);
      ev.code = 8; tp_st = 0; mg_event(&ev);
      ev.code = 1; tp_st = 1; tp_x = 30; tp_y = 150; mg_event(&ev); ev.code = 7; mg_event(&ev); printf("tap left of board -> tap=%d\n", m->tap); m->tap = 0; m->x = 30; m->y = 150; ev.code = 8; mg_event(&ev); }
    return 0;
}
