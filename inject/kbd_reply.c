/* On-screen keyboard for notification replies (Galaxy Fit3 AZA3).
 * Hook: bl send_reply(seq_id, text, len) @0x2c1d0802. If the chosen quick reply is exactly "..." we open a fullscreen
 * 256x402 LVGL image canvas with our keyboard instead of sending; OK sends the typed UTF-8 text through the firmware's
 * own send_reply(); X cancels. All state lives in malloc'd memory (flash cave is read-only: no globals!). */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "kbd.c"
#define MOTOR_ONCE ((int (*)(int, int))0x2c1423f9)
#define EV_TARGET ((void *(*)(void *))0x2c27d981)
#define EV_CODE ((int (*)(void *))0x2c27d989)
#define EV_USER ((void *(*)(void *))0x2c27d995)
#define TP_SAMPLE_GET ((int (*)(void *))0x2c0a76d9)
#define MALLOC ((void *(*)(u32))0x2c0eb00d)
#define FREE ((void (*)(void *))0x2c0eb019)
#define SEND_REPLY ((int (*)(u32, const char *, u32))0x2c112f59)
#define IMG_CREATE ((void *(*)(void *))0x2c2905ad)
#define IMG_SET_SRC ((void (*)(void *, const void *))0x2c29064d)
#define OBJ_ALIGN_TO ((void (*)(void *, void *, int, int, int))0x2c27908d)
#define ADD_FLAG ((void (*)(void *, u32))0x2c27cbb9)
#define CLEAR_FLAG ((void (*)(void *, u32))0x2c27cc5d)
#define ADD_EVENT_CB ((void *(*)(void *, void *, int, void *))0x2c27d9cd)
#define INVALIDATE ((void (*)(void *))0x2c2782a9)
#define REPLY_PAGE_ROOT (*(void **)0x201160a0)

typedef struct { u32 dsc[4]; Kbd k; u32 seq; int x, y; void *img; u32 *fbuf; } St;

static void on_event(void *e) {
    int code = EV_CODE(e);
    St *st = (St *)EV_USER(e); void *img = EV_TARGET(e);
    if (code == 1 || code == 2) {                                     /* PRESSED / PRESSING: remember finger position */
        u8 s[64]; for (int i = 0; i < 64; i++) s[i] = 0;
        TP_SAMPLE_GET(s);
        if (s[1] == 1 || s[1] == 2) { st->x = *(u16 *)(s + 4); st->y = *(u16 *)(s + 6); }
        return;
    }
    if (code != 7) return;                                            /* CLICKED = one key tap */
    kbd_touch(&st->k, st->x, st->y);
    if (st->k.state == KBD_EDIT) { kbd_draw(&st->k, (u16 *)st->dsc[2]); INVALIDATE(img); return; }
    if (st->k.state == KBD_SEND) { SEND_REPLY(st->seq, st->k.buf, (u32)st->k.len); MOTOR_ONCE(1, 0); }
    ADD_FLAG(img, 1);                                                 /* hide; page destroys it with its parent */
    FREE(st->fbuf);                                                   /* give the 205 KB frame back (st stays: tiny, text may be referenced) */
}

int kbd_hook(u32 seq, const char *text, u32 len) {
    int dots = text[0] == '.' && text[1] == '.' && text[2] == '.', ell = (u8)text[0] == 0xE2 && (u8)text[1] == 0x80 && (u8)text[2] == 0xA6;
    if (len != 3 || !(dots || ell)) return 0;                          /* normal quick reply: send as is */
    void *root = REPLY_PAGE_ROOT; if (!root) { MOTOR_ONCE(1, 0); return 1; }
    St *st = (St *)MALLOC(sizeof(St)); if (!st) { MOTOR_ONCE(5, 0); return 1; }
    u32 *fbuf = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!fbuf) { FREE(st); MOTOR_ONCE(5, 0); return 1; }
    kbd_init(&st->k, ""); st->seq = seq; st->x = st->y = 0; st->fbuf = fbuf;
    u16 *px = (u16 *)((u8 *)fbuf + 8);
    st->dsc[0] = 4u | ((u32)KBD_W << 10) | ((u32)KBD_H << 21); st->dsc[1] = KBD_W * KBD_H * 2; st->dsc[2] = (u32)px;
    kbd_draw(&st->k, px);
    void *img = IMG_CREATE(root); st->img = img;
    IMG_SET_SRC(img, st->dsc);
    OBJ_ALIGN_TO(img, 0, 1, 0, 0);                                    /* top-left, covers the page */
    ADD_FLAG(img, 2); CLEAR_FLAG(img, 0x10);                          /* clickable, not scrollable */
    ADD_EVENT_CB(img, (void *)on_event, 0, st);
    MOTOR_ONCE(69, 0);
    return 1;
}
