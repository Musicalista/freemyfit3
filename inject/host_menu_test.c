#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz, freed;
static void *g_user; static int tp_down, tp_x, tp_y; static unsigned fake_lang = 68;
#define UI_LANG_ID (fake_lang)
#define MOTOR_ONCE(a, b) (buzz++, 0)
#define MALLOC(n) malloc(n)
#define FREE(p) (freed++, free(p))
#define TICK_GET() (fake_now)
#define INVALIDATE(o) ((void)0)
#define ADD_FLAG(o, f) ((void)0)
#define CLEAR_FLAG(o, f) ((void)0)
#define IMG_CREATE(r) ((void *)1)
#define IMG_SET_SRC(i, s) ((void)0)
#define OBJ_ALIGN_TO(a, b, c, d, e) ((void)0)
#define ADD_EVENT_CB(o, cb, f, u) (g_user = (u), (void *)0)
#define TIMER_DEL(t) ((void)0)
#define TIMER_CREATE(cb, p, u) (memcpy(fake_timer + 0xC, &(u), sizeof(void *)), (void *)fake_timer)
typedef struct { int code; void *user; } Ev;
#define EV_CODE(e) (((Ev *)(e))->code)
#define EV_USER(e) (((Ev *)(e))->user)
static int fake_tp(unsigned char *s) { s[1] = tp_down ? 1 : 0; *(uint16_t *)(s + 4) = (uint16_t)tp_x; *(uint16_t *)(s + 6) = (uint16_t)tp_y; return 0; }
#define TP_SAMPLE_GET(s) fake_tp(s)
#define SEND_REPLY(a, b, c) 0
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static void fire(void *user, int code) { Ev e = { code, user }; menu_event(&e); }
static void tap(void *user, int x, int y) { tp_down = 1; tp_x = x; tp_y = y; fire(user, 1); tp_down = 0; fire(user, 8); fire(user, 7); }
static void swipe(void *user, int x0, int y0, int x1, int y1) {
    tp_down = 1; tp_x = x0; tp_y = y0; fire(user, 1); tp_x = (x0 + x1) / 2; tp_y = (y0 + y1) / 2; fire(user, 2); tp_x = x1; tp_y = y1; fire(user, 2);
    tp_down = 0; fire(user, 8); fire(user, 7);                                   /* LVGL sends CLICKED after RELEASED even for a swipe */
}
static void shots(const char *tag) {
    static const unsigned langs[2] = { 68, 30 }; static const char *names[2] = { "pt", "en" };
    for (int l = 0; l < 2; l++) {
        fake_lang = langs[l]; menu_open((void *)1); MSt *m = (MSt *)g_user; char n[64];
        sprintf(n, "menu_%s_%s_p1.ppm", tag, names[l]); dump(m->px, n);
        if (MT_PAGES > 1) { m->page = 1; menu_draw(m); sprintf(n, "menu_%s_%s_p2.ppm", tag, names[l]); dump(m->px, n); }
        m->page = 0; m->mode = 1; menu_draw(m); sprintf(n, "menu_%s_%s_help.ppm", tag, names[l]); dump(m->px, n);
    }
    fake_lang = 68;
}
int main(void) {
    int fails = 0;
#ifdef NO_GAMES
    shots("nogames"); printf("tiles=%d pages=%d\n", MT_N, MT_PAGES); return 0;
#else
    shots("games");
#endif
    menu_open((void *)1); MSt *m = (MSt *)g_user;
    printf("page=%d mode=%d\n", m->page, m->mode); dump(m->px, "menu_p1.ppm");
    swipe(m, 200, 180, 40, 190);
    printf("after swipe left: page=%d (want 1), no app opened: %d\n", m->page, g_user == m); if (m->page != 1 || g_user != m) fails++;
    dump(m->px, "menu_p2.ppm");
    swipe(m, 40, 180, 210, 185); printf("after swipe right: page=%d (want 0)\n", m->page); if (m->page != 0) fails++;
    tap(m, 208, 375); printf("next arrow: page=%d (want 1)\n", m->page); if (m->page != 1) fails++;
    tap(m, 48, 375); printf("prev arrow: page=%d (want 0)\n", m->page); if (m->page != 0) fails++;
    printf("a mostly-vertical drag is not a swipe: "); swipe(m, 100, 150, 110, 260); printf("page=%d (want 0)\n", m->page); if (m->page != 0) fails++;
    tap(m, 208, 375);                                                            /* page 2: Texto, Ajuda, Doom, Fechar */
    tap(m, 188, 90); printf("tap Ajuda: mode=%d (want 1)\n", m->mode); if (m->mode != 1) fails++; dump(m->px, "menu_help.ppm");
    tap(m, 100, 100); printf("tap to leave help: mode=%d (want 0)\n", m->mode); if (m->mode != 0) fails++;
    int fr = freed; tap(m, 188, 152); printf("tap Fechar: freed buffers=%d (want >0), buzz=%d\n", freed - fr, buzz); if (freed == fr) fails++;
    menu_open((void *)1); m = (MSt *)g_user; void *before = g_user;
    tap(m, 68, 90); printf("tap Snake: launched a new screen=%d (want 1)\n", g_user != before); if (g_user == before) fails++;
    printf(fails ? "FAILED %d\n" : "ALL OK\n", fails);
    return fails;
}
