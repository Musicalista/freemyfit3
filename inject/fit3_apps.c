/* Fit3 injected apps (AZA3): on-screen reply keyboard + native 3D game (rd-132211 port).
 * Hook: bl send_reply(seq_id, text, len) @0x2c1d0802 (quick-reply click handler). Quick reply "..." -> keyboard, "3d" -> game.
 * Flash cave is READ-ONLY: no writable globals; all state is malloc'd. */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
typedef signed char int8_t_;
#include <stdint.h>
#ifndef NO_GAMES
#include "m3d.c"
#include "rd.c"
#endif
#include "kbd.c"
#ifndef NO_GAMES
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

static int menu_open(void *root);
#ifndef NO_GAMES
#ifdef GAME_CS
#define RW 128
#define RH 201
#define GAME_LABEL "FPS CS"
#include "cs_game.inc.c"
#else
#define GAME_LABEL "3D"
/* ================= 3D game ================= */
#define RW 128
#define RH 201                 /* internal render size; blitted 2x to the 256x402 canvas */
typedef struct {
    u32 dsc[4]; u16 *cv; u16 *rfb, *zb; Rd *g; void *img, *timer; u32 *cvblk; void *root; int from_menu;
    int pressed, x, y; u32 last_tick; int fps, frames; u32 fps_t; M3dTex tex;
    int moved, sx, sy, lx, ly, v0;                       /* drag-to-look: v0 = the touch started on the view (not on a button) */
} GSt;
typedef struct { int x, y, w, h; char label; int key; } Btn;
enum { B_LU, B_LD, B_JUMP, B_RESET, B_TL, B_FWD, B_BACK, B_TR, B_N };
static const Btn btns[B_N] = {
    {4, 262, 60, 56, 'U', 0}, {68, 262, 60, 56, 'D', 0}, {132, 262, 60, 56, 'J', 0}, {196, 262, 56, 56, 'R', 0},
    {4, 326, 60, 56, '<', 0}, {68, 326, 60, 56, '^', 0}, {132, 326, 60, 56, 'v', 0}, {196, 326, 56, 56, '>', 0} };
static int hit_btn(int x, int y) {
    for (int i = 0; i < B_N; i++) if (x >= btns[i].x && x < btns[i].x + btns[i].w && y >= btns[i].y && y < btns[i].y + btns[i].h) return i;
    return -1;
}
static int in_close(int x, int y) { return x >= 216 && y < 40; }

static int menu_open(void *root);
static void game_close(GSt *st) {
    TIMER_DEL(st->timer); ADD_FLAG(st->img, 1);
    FREE(st->cvblk); FREE(st->rfb); FREE(st->zb); FREE(st->g);
    if (st->from_menu) menu_open(st->root);                              /* back to the Apps extras menu */
}
static void game_event(void *e) {
    int code = EV_CODE(e); GSt *st = (GSt *)EV_USER(e); int x, y;
    if (code == 1 || code == 2) {
        if (touch_read(&x, &y)) {
            if (code == 1 || !st->pressed) { st->sx = st->lx = x; st->sy = st->ly = y; st->moved = 0; st->v0 = hit_btn(x, y) < 0 && !in_close(x, y); }
            st->pressed = 1; st->x = x; st->y = y;
            if (x - st->sx > 12 || st->sx - x > 12 || y - st->sy > 12 || st->sy - y > 12) st->moved = 1;
        }
        return;
    }
    if (code == 8 || code == 3) { st->pressed = 0; return; }
    if (code == 7) {                                                      /* tap (a drag is not a tap: it looks around) */
        if (in_close(st->x, st->y)) { game_close(st); return; }
        if (!st->moved && hit_btn(st->x, st->y) < 0) rd_key(st->g, RD_BREAK, 1);
    } else if (code == 5) {                                               /* long press on the view: place block */
        if (!st->moved && hit_btn(st->x, st->y) < 0 && !in_close(st->x, st->y)) rd_key(st->g, RD_PLACE, 1);
    }
}
static void game_ui(GSt *st) {
    u16 *cv = st->cv; int hb = st->pressed ? hit_btn(st->x, st->y) : -1;
    for (int i = 0; i < B_N; i++) {
        const Btn *b = &btns[i]; uint16_t bg = (i == hb) ? C(220, 160, 30) : C(40, 44, 60);
        rect(cv, b->x, b->y, b->w, b->h, bg); rect(cv, b->x, b->y, b->w, 2, C(120, 130, 160));
        put(cv, b->x + b->w / 2 - 8, b->y + b->h / 2 - 12, (uint32_t)(uint8_t)b->label, 2, C(255, 255, 255));
    }
    rect(cv, 216, 0, 40, 40, C(170, 30, 30)); put(cv, 228, 8, 'X', 2, C(255, 255, 255));
    { int v = st->fps, d1 = v / 10 % 10, d0 = v % 10; rect(cv, 0, 0, 40, 28, C(0, 0, 0)); put(cv, 2, 2, (uint32_t)('0' + d1), 2, C(255, 255, 0)); put(cv, 18, 2, (uint32_t)('0' + d0), 2, C(255, 255, 0)); }
}
static void game_tick(void *timer) {
    GSt *st = *(GSt **)((u8 *)timer + 0xC); Rd *g = st->g;
    u32 now = TICK_GET();
    int hb = st->pressed ? hit_btn(st->x, st->y) : -1;
    if (st->pressed && st->v0 && st->moved) rd_look(g, (float)(st->x - st->lx) * 3.0f, (float)(st->ly - st->y) * 3.0f);   /* drag on the view = look around */
    st->lx = st->x; st->ly = st->y;
    rd_advance(g, (int64_t)now); int n = g->ticks; if (n > 3) n = 3;
    for (int i = 0; i < n; i++) {
        rd_key(g, RD_FWD, hb == B_FWD); rd_key(g, RD_BACK, hb == B_BACK); rd_key(g, RD_JUMP, hb == B_JUMP); rd_key(g, RD_RESET, hb == B_RESET);
        if (hb == B_TL) rd_key(g, RD_TURN_L, 1); if (hb == B_TR) rd_key(g, RD_TURN_R, 1);
        if (hb == B_LU) rd_key(g, RD_LOOK_UP, 1); if (hb == B_LD) rd_key(g, RD_LOOK_DOWN, 1);
        rd_tick(g);
    }
    rd_render(g, g->alpha, &st->tex, RW, RH, st->rfb, st->zb);
    for (int y = 0; y < RH; y++) {                                        /* 2x nearest blit */
        const u16 *s = st->rfb + y * RW; u32 *d0 = (u32 *)(st->cv + (2 * y) * KBD_W), *d1 = (u32 *)(st->cv + (2 * y + 1) * KBD_W);
        for (int x = 0; x < RW; x++) { u32 p = s[x]; p |= p << 16; d0[x] = p; d1[x] = p; }
    }
    st->frames++; if (now - st->fps_t >= 1000) { st->fps = st->frames; st->frames = 0; st->fps_t = now; }
    game_ui(st); INVALIDATE(st->img);
}
static int game_open(void *root, int from_menu) {
    GSt *st = (GSt *)MALLOC(sizeof(GSt)); if (!st) return 0;
    u32 *cvblk = (u32 *)MALLOC(KBD_W * KBD_H * 2 + 8); u16 *rfb = (u16 *)MALLOC(RW * RH * 2), *zb = (u16 *)MALLOC(RW * RH * 2); Rd *g = (Rd *)MALLOC(sizeof(Rd));
    if (!cvblk || !rfb || !zb || !g) { if (cvblk) FREE(cvblk); if (rfb) FREE(rfb); if (zb) FREE(zb); if (g) FREE(g); FREE(st); return 0; }
    u32 now = TICK_GET(); rd_init(g, (int64_t)now); g->last_ms = (int64_t)now;
    st->root = root; st->from_menu = from_menu; st->g = g; st->rfb = rfb; st->zb = zb; st->cvblk = cvblk; st->cv = (u16 *)((u8 *)cvblk + 8); st->pressed = 0; st->x = st->y = 0;
    st->tex.pix = rd_tex_pix; st->tex.w = 32; st->tex.h = 16; st->fps = 0; st->frames = 0; st->fps_t = now;
    game_ui(st);
    st->img = make_canvas(root, st->dsc, st->cv, (void *)game_event, st);
    st->timer = TIMER_CREATE((void *)game_tick, 33, st);
    return 1;
}

#endif
#endif /* NO_GAMES */

#include "common.inc.c"
#if !defined(NO_GAMES) && !defined(NO_DOOM)
#include "doom.inc.c"
#define HAVE_DOOM 1
#endif
#if !defined(NO_GAMES) || defined(MINI_GAMES)           /* Snake, Flappy, Tetris, 2048: small, so they fit next to the Internet app */
#include "minigames.inc.c"
#define HAVE_MINIGAMES 1
#endif
#ifndef NO_WEB                                         /* text reader, remote browser, Internet and AI all live here */
#define HAVE_WEB 1
#include "webreader.inc.c"
#include "browser.inc.c"
#endif
#include "menu.inc.c"

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
