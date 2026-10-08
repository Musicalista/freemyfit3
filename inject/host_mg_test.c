#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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
#define TP_SAMPLE_GET(s) 0
#define SEND_REPLY(a, b, c) 0
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static MG *start(int kind) { mg_open((void *)1, kind, 0); return *(MG **)(fake_timer + 0xC); }
static void run(MG *m, int n) { for (int i = 0; i < n; i++) { fake_now += 33; mg_tick(fake_timer); } }
int main(void) {
    printf("sizeof(MG)=%zu\n", sizeof(MG));
    /* ---- Snake: steer toward the food by tapping ---- */
    MG *m = start(MG_SNAKE); Snake *s = &m->g.s;
    for (int f = 0; f < 4000 && !s->over; f++) {
        int hx = s->bx[s->head & 255], hy = s->by[s->head & 255], want;
        if (0) printf("  f=%d head=(%d,%d) food=(%d,%d) dir=%d nd=%d len=%d\n", f, hx, hy, s->fx, s->fy, s->dir, s->nd, s->len);
        if (s->fx > hx) want = 1; else if (s->fx < hx) want = 3; else want = s->fy > hy ? 2 : 0;
        if (((want + 2) & 3) == s->dir) want = (s->dir + 1) & 3;
        if (want != s->dir) { m->x = ((s->dir + 1) & 3) == want ? 200 : 20; m->tap = 1; }
        run(m, 1);
    }
    printf("snake: score=%d len=%d over=%d\n", s->score, s->len, s->over); dump(m->px, "mg_snake.ppm");
    /* ---- Flappy: flap when below the gap centre ---- */
    m = start(MG_FLAPPY); Flappy *fl = &m->g.f; m->tap = 1; run(m, 1);
    for (int f = 0; f < 3000 && !fl->over; f++) {
        int tgt = 200; for (int i = 0; i < 3; i++) if (fl->px[i] + 44 > 55) { tgt = fl->gy[i] + 56; break; }
        if (fl->y + 10 > (float)tgt && fl->vy > -1.0f) m->tap = 1;
        run(m, 1);
    }
    printf("flappy: score=%d over=%d\n", fl->score, fl->over); dump(m->px, "mg_flappy.ppm");
    /* ---- Tetris: drop pieces with a naive bot that fills rows ---- */
    m = start(MG_TETRIS); Tetris *t = &m->g.t; int placed = 0;
    for (int f = 0; f < 20000 && !t->over && placed < 60; f++) {
        m->pressed = 1; m->x = 220; m->y = 385; run(m, 1); m->pressed = 0;                 /* hold soft drop */
        if (t->y < 1 && t->t == 0) { placed++; }
    }
    printf("tetris: score=%d lines=%d level=%d over=%d\n", t->score, t->lines, t->level, t->over); dump(m->px, "mg_tetris.ppm");
    /* ---- Tetris line clear: a row with a gap that the next piece fills ---- */
    m = start(MG_TETRIS); t = &m->g.t; for (int x = 0; x < 9; x++) t->board[19][x] = 3; t->cur = tt_rot(tt_shapes[0]); t->shape = 0; t->x = 7; t->y = 16;
    for (int f = 0; f < 200 && t->lines == 0; f++) run(m, 1);
    printf("tetris line clear test: lines=%d score=%d row19 empty=%d\n", t->lines, t->score, t->board[19][0] == 0);
    return 0;
}
