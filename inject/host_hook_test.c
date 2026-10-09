#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 1000;
static unsigned char fake_timer[64]; static int buzz, freed, sent;
static void *g_user; static unsigned fake_lang = 30; static void *fake_root = (void *)1;
#define UI_LANG_ID (fake_lang)
#define REPLY_PAGE_ROOT (fake_root)
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
#define EV_TARGET(e) ((void *)1)
static int tp_down, tp_x, tp_y;
static int fake_tp(unsigned char *s) { s[1] = tp_down ? 1 : 0; *(uint16_t *)(s + 4) = (uint16_t)tp_x; *(uint16_t *)(s + 6) = (uint16_t)tp_y; return 0; }
#define TP_SAMPLE_GET(s) fake_tp(s)
static char last_sent[300]; static unsigned last_len;
static int fake_send(unsigned seq, const char *t, unsigned len) { (void)seq; sent++; memcpy(last_sent, t, len); last_sent[len] = 0; last_len = len; return 0; }
#define SEND_REPLY(a, b, c) fake_send(a, b, c)
#include "fit3_apps.c"
static void tapk(KSt *st, int x, int y) { Ev e = { 1, st }; tp_down = 1; tp_x = x; tp_y = y; e.code = 1; kbd_event(&e); tp_down = 0; e.code = 7; kbd_event(&e); fake_now += 1500; }
int main(void) {
    int fails = 0, x, y; unsigned char rec[0x600]; memset(rec, 0, sizeof rec);
    strcpy((char *)rec + 0x120 + 0x119, "Maria"); strcpy((char *)rec + 0x120 + 0x35c, "Vai chegar a que horas?");

    int r = kbd_hook(7, "Ok, ja chego", 12, rec); KSt *st = (KSt *)g_user;
    printf("quick reply tap: hook=%d, keyboard text=\"%s\" (want hook=1, \"Ok, ja chego\")\n", r, st->k.buf); if (r != 1 || strcmp(st->k.buf, "Ok, ja chego")) fails++;
    printf("language flag en=%d (watch language 30 = English -> want 1)\n", st->k.en); if (!st->k.en) fails++;
    kbd_find_action(&st->k, KA_BACK, &x, &y); tapk(st, x, y); tapk(st, x, y);                   /* edit: delete 2 chars */
    kbd_find_t9(&st->k, 5, &x, &y); tapk(st, x, y);                                              /* add 'j' */
    kbd_find_action(&st->k, KA_SEND, &x, &y); tapk(st, x, y);
    printf("after edit + OK: sent=%d text=\"%s\" (want 1, \"Ok, ja chej\")\n", sent, last_sent); if (sent != 1 || strcmp(last_sent, "Ok, ja chej")) fails++;

    r = kbd_hook(8, "...", 3, rec); st = (KSt *)g_user; printf("marker '...': hook=%d text=\"%s\" (want 1, empty)\n", r, st->k.buf); if (r != 1 || st->k.buf[0]) fails++;
    fake_lang = 68; r = kbd_hook(9, "Sim", 3, rec); st = (KSt *)g_user; printf("pt-BR watch: en=%d (want 0)\n", st->k.en); if (st->k.en) fails++;
    fake_root = 0; int before = sent; r = kbd_hook(10, "Sim", 3, rec); printf("no page to draw on: hook=%d (0 = send the quick reply normally), sent=%d\n", r, sent - before); if (r != 0) fails++;
    printf(fails ? "FAILED %d\n" : "ALL OK\n", fails);
    return fails;
}
