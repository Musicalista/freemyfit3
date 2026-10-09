/* Fit3 injected apps (AZA3): on-screen reply keyboard + native 3D game (rd-132211 port).
 * Hook: bl send_reply(seq_id, text, len) @0x2c1d0802 (quick-reply click handler). Quick reply "..." -> keyboard, "3d" -> game.
 * Flash cave is READ-ONLY: no writable globals; all state is malloc'd. */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
typedef signed char int8_t_;
#include <stdint.h>
#ifndef NO_GAMES
#include "m3d.c"
#ifdef GAME_CS
#include "rd.c"
#else
#include "vox.c"
#endif
#endif
#include "kbd.c"
#if !defined(NO_GAMES) && defined(GAME_CS)
#include "rd_tex.h"
#endif

#ifndef MOTOR_ONCE
#define MOTOR_ONCE ((int (*)(int, int))0x2c1423f9)
#endif
#ifndef EV_TARGET
#define EV_TARGET ((void *(*)(void *))0x2c27d981)
#endif
#ifndef EV_CODE
#define EV_CODE ((int (*)(void *))0x2c27d989)
#endif
#ifndef EV_USER
#define EV_USER ((void *(*)(void *))0x2c27d995)
#endif
#ifndef TP_SAMPLE_GET
#define TP_SAMPLE_GET ((int (*)(void *))0x2c0a76d9)
#endif
#ifndef MALLOC
#define MALLOC ((void *(*)(u32))0x2c0eb00d)
#endif
#ifndef FREE
#define FREE ((void (*)(void *))0x2c0eb019)
#endif
#ifndef SEND_REPLY
#define SEND_REPLY ((int (*)(u32, const char *, u32))0x2c112f59)
#endif
#ifndef IMG_CREATE
#define IMG_CREATE ((void *(*)(void *))0x2c2905ad)
#endif
#ifndef IMG_SET_SRC
#define IMG_SET_SRC ((void (*)(void *, const void *))0x2c29064d)
#endif
#ifndef OBJ_ALIGN_TO
#define OBJ_ALIGN_TO ((void (*)(void *, void *, int, int, int))0x2c27908d)
#endif
#ifndef ADD_FLAG
#define ADD_FLAG ((void (*)(void *, u32))0x2c27cbb9)
#endif
#ifndef CLEAR_FLAG
#define CLEAR_FLAG ((void (*)(void *, u32))0x2c27cc5d)
#endif
#ifndef ADD_EVENT_CB
#define ADD_EVENT_CB ((void *(*)(void *, void *, int, void *))0x2c27d9cd)
#endif
#ifndef INVALIDATE
#define INVALIDATE ((void (*)(void *))0x2c2782a9)
#endif
#ifndef TIMER_CREATE
#define TIMER_CREATE ((void *(*)(void *, u32, void *))0x2c28a3f1)
#endif
#ifndef TIMER_DEL
#define TIMER_DEL ((void (*)(void *))0x2c28a449)
#endif
#ifndef TICK_GET
#define TICK_GET ((u32 (*)(void))0x2c288ef5)
#endif
#ifndef REPLY_PAGE_ROOT
#define REPLY_PAGE_ROOT (*(void **)0x201160a0)
#endif

/* ---------------- UI language: follows the watch setting (byte @0x200fafe0 via getter 0x2c212160; 68 = pt-BR, 52 = pt-PT, anything else -> English) ---------------- */
#ifndef UI_LANG_ID
#define UI_LANG_ID (((u32 (*)(void))0x2c212161)())
#endif
static int ui_pt(void) { u32 id = UI_LANG_ID; return id == 68 || id == 52; }
#define TR(pt, en) (ui_pt() ? (pt) : (en))

/* ---------------- shared: sample the touch panel ---------------- */
static int touch_read(int *x, int *y) {
    u8 s[64]; for (int i = 0; i < 64; i++) s[i] = 0;
    TP_SAMPLE_GET(s);
    if (s[1] == 1 || s[1] == 2) { *x = *(u16 *)(s + 4); *y = *(u16 *)(s + 6); return 1; }
    return 0;
}
static void *make_canvas(void *root, u32 *dsc, u16 *px, void *cb, void *user) {
    dsc[0] = 4u | ((u32)KBD_W << 10) | ((u32)KBD_H << 21); dsc[1] = KBD_W * KBD_H * 2; dsc[2] = (u32)px;
    void *img = IMG_CREATE(root);
    IMG_SET_SRC(img, dsc);
    OBJ_ALIGN_TO(img, 0, 1, 0, 0);
    ADD_FLAG(img, 2); CLEAR_FLAG(img, 0x10);
    ADD_EVENT_CB(img, cb, 0, user);
    return img;
}

/* ================= keyboard ================= */
typedef struct { u32 dsc[4]; Kbd k; u32 seq; int x, y; void *img; u32 *fbuf; u16 *px; void *done, *ctx; } KSt;   /* done != 0: call done(ctx, text) instead of sending a reply */

static void kbd_event(void *e) {
    int code = EV_CODE(e); KSt *st = (KSt *)EV_USER(e); void *img = EV_TARGET(e);
    if (code == 1 || code == 2) { int x, y; if (touch_read(&x, &y)) { st->x = x; st->y = y; } return; }
    if (code != 7) return;
    kbd_touch(&st->k, st->x, st->y, TICK_GET());
    if (st->k.state == KBD_EDIT) { kbd_draw(&st->k, st->px); INVALIDATE(img); return; }
    if (st->k.state == KBD_SEND) { if (st->done) ((void (*)(void *, const char *))st->done)(st->ctx, st->k.buf); else { SEND_REPLY(st->seq, st->k.buf, (u32)st->k.len); MOTOR_ONCE(1, 0); } }
    ADD_FLAG(img, 1); FREE(st->fbuf);
}
static int kbd_open(void *root, u32 seq, const u8 *rec, const char *init) {
    KSt *st = (KSt *)MALLOC(sizeof(KSt)); if (!st) return 0;
    u32 *fbuf = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!fbuf) { FREE(st); return 0; }
    kbd_init(&st->k, init); st->k.en = (uint8_t)!ui_pt();
    if (rec) kbd_set_msg(&st->k, (const char *)rec + 0x120 + 0x119, (const char *)rec + 0x120 + 0x35c);   /* title / body inside the notice record */
    st->seq = seq; st->x = st->y = 0; st->fbuf = fbuf; st->done = 0; st->ctx = 0;
    u16 *px = (u16 *)((u8 *)fbuf + 8); st->px = px; kbd_draw(&st->k, px);
    st->img = make_canvas(root, st->dsc, px, (void *)kbd_event, st);
    return 1;
}

static int kbd_open_cb(void *root, const char *title, const char *hint, void *done, void *ctx) {
    KSt *st = (KSt *)MALLOC(sizeof(KSt)); if (!st) return 0;
    u32 *fbuf = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); if (!fbuf) { FREE(st); return 0; }
    kbd_init(&st->k, ""); st->k.en = (uint8_t)!ui_pt(); kbd_set_msg(&st->k, title, hint); st->seq = 0; st->x = st->y = 0; st->fbuf = fbuf; st->done = done; st->ctx = ctx;
    u16 *px = (u16 *)((u8 *)fbuf + 8); st->px = px; kbd_draw(&st->k, px);
    st->img = make_canvas(root, st->dsc, px, (void *)kbd_event, st);
    return 1;
}

#include "common.inc.c"
static int menu_open(void *root);
#ifndef NO_GAMES
#ifdef GAME_CS
#define RW 128
#define RH 201
#define GAME_LABEL "FPS CS"
#include "cs_game.inc.c"
#else
#include "mc_game.inc.c"
#endif
#endif /* NO_GAMES */

#if !defined(NO_GAMES) && !defined(NO_DOOM)
#include "doom.inc.c"
#define HAVE_DOOM 1
#endif
#if !defined(NO_GAMES) || defined(MINI_GAMES)           /* Snake, Flappy, Tetris, 2048: small, so they fit next to the Internet app */
#include "minigames.inc.c"
#define HAVE_MINIGAMES 1
#endif
#ifdef WITH_TOOLS                                      /* utilities: calculator, stopwatch/timer, flashlight, notes, counter, dice */
#include "tools.inc.c"
#define HAVE_TOOLS 1
#endif
#ifdef WITH_GB                                         /* Game Boy / Game Boy Color emulator (cartridge from /user/gb.gb) */
#include "gbcore.inc.c"
#include "gb.inc.c"
#define HAVE_GB 1
#endif
#ifndef NO_WEB                                         /* text reader, remote browser, Internet and AI all live here */
#define HAVE_WEB 1
#include "webreader.inc.c"
#include "browser.inc.c"
#endif
#ifdef KBD_ONLY
int menu_hook(void *root, int page) { (void)root; (void)page; return 0; }          /* no launcher in the keyboard-only build */
#else
#include "menu.inc.c"
#endif

/* ================= hook ================= */
/* Quick-reply tap hook. By default EVERY quick reply opens the keyboard, pre-filled with that reply's text (edit it, or clear it and type; OK sends).
 * The markers "..." / "…" open it empty. Build with -DKBD_MARKER_ONLY to restore the old behaviour (only the markers open the keyboard). */
int kbd_hook(u32 seq, const char *text, u32 len, const u8 *rec) {
    int dots = len == 3 && text[0] == '.' && text[1] == '.' && text[2] == '.';
    int ell = len == 3 && (u8)text[0] == 0xE2 && (u8)text[1] == 0x80 && (u8)text[2] == 0xA6;
#ifdef NO_GAMES
    int game = 0;
#else
    int game = len == 2 && (text[0] == '3') && (text[1] == 'd' || text[1] == 'D');
#endif
    int marker = dots || ell || game;
#ifdef KBD_MARKER_ONLY
    if (!marker) return 0;                                              /* normal quick reply: send as is */
#endif
    void *root = REPLY_PAGE_ROOT;
    if (!root) { if (marker) MOTOR_ONCE(1, 0); return marker; }         /* no page to draw on: a real quick reply is still sent normally */
    char init[KBD_MAX + 4]; u32 n = 0;
    if (!marker && text && len <= KBD_MAX) { while (n < len) { init[n] = text[n]; n++; } }
    init[n] = 0;
#ifdef NO_GAMES
    int ok = kbd_open(root, seq, rec, init);
#else
    int ok = game ? game_open(root, 0) : kbd_open(root, seq, rec, init);
#endif
    MOTOR_ONCE(ok ? 69 : 5, 0);
    if (!ok && !marker) return 0;                                       /* out of memory: fall back to sending the quick reply */
    return 1;
}
