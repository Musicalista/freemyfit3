/* Probe 4: fullscreen 256x402 RGB565 canvas via LVGL image + touch painting.
 * Trigger: CLICK on an item of Settings>Vibration (hook bl lv_event_get_code @0x2c1b4b26).
 * Canvas: dark-blue bg, 64px grid, white border, red close box top-right (x>=216,y<40). Finger paints white 8x8 squares.
 * Buzz: 69 = opened, 5 = malloc(205840) failed, 1 = no page. */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#define MOTOR_ONCE ((int (*)(int, int))0x2c1423f9)
#define EV_TARGET ((void *(*)(void *))0x2c27d981)
#define EV_CODE ((int (*)(void *))0x2c27d989)
#define EV_USER ((void *(*)(void *))0x2c27d995)
#define TP_SAMPLE_GET ((int (*)(void *))0x2c0a76d9)
#define MALLOC ((void *(*)(u32))0x2c0eb00d)
#define IMG_CREATE ((void *(*)(void *))0x2c2905ad)
#define IMG_SET_SRC ((void (*)(void *, const void *))0x2c29064d)
#define OBJ_ALIGN_TO ((void (*)(void *, void *, int, int, int))0x2c27908d)
#define ADD_FLAG ((void (*)(void *, u32))0x2c27cbb9)
#define CLEAR_FLAG ((void (*)(void *, u32))0x2c27cc5d)
#define ADD_EVENT_CB ((void *(*)(void *, void *, int, void *))0x2c27d9cd)
#define INVALIDATE ((void (*)(void *))0x2c2782a9)
#define W 256
#define H 402
#define RGB(r, g, b) ((u16)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

static void on_event(void *e) {
    int code = EV_CODE(e);
    if (code != 1 && code != 2 && code != 8) return;            /* PRESSED, PRESSING, RELEASED */
    u8 s[128]; for (int i = 0; i < 128; i++) s[i] = 0;
    TP_SAMPLE_GET(s);
    if (s[1] != 1 && s[1] != 2) return;
    int x = *(u16 *)(s + 4), y = *(u16 *)(s + 6);
    u32 *blk = (u32 *)EV_USER(e); void *img = EV_TARGET(e);
    if (x >= 216 && y < 40) { ADD_FLAG(img, 1); return; }        /* close: LV_OBJ_FLAG_HIDDEN */
    u16 *px = (u16 *)blk[2];
    for (int j = y - 4; j < y + 4; j++) for (int i = x - 4; i < x + 4; i++) if (i >= 0 && i < W && j >= 0 && j < H) px[j * W + i] = 0xFFFF;
    INVALIDATE(img);
}

void probe(void *ev) {
    if (EV_CODE(ev) != 7) return;                                 /* LV_EVENT_CLICKED */
    void **pg = *(void ***)0x20115f3c; if (!pg || !pg[0]) { MOTOR_ONCE(1, 0); return; }
    u32 *blk = (u32 *)MALLOC(16 + W * H * 2); if (!blk) { MOTOR_ONCE(5, 0); return; }
    u16 *px = (u16 *)((u8 *)blk + 16);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        u16 c = RGB(0, 0, 60);
        if ((x & 63) == 0 || (y & 63) == 0) c = RGB(40, 40, 120);
        if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2) c = 0xFFFF;
        if (x >= 216 && y < 40) c = RGB(200, 0, 0);
        px[y * W + x] = c;
    }
    for (int k = 0; k < 24; k++) { px[(8 + k) * W + 220 + k] = 0xFFFF; px[(8 + k) * W + 244 - k] = 0xFFFF; }   /* X mark */
    blk[0] = 4u | ((u32)W << 10) | ((u32)H << 21); blk[1] = W * H * 2; blk[2] = (u32)px;
    void *img = IMG_CREATE(pg[0]);
    IMG_SET_SRC(img, blk);
    OBJ_ALIGN_TO(img, 0, 1, 0, 0);                                /* LV_ALIGN_TOP_LEFT */
    ADD_FLAG(img, 2);                                             /* CLICKABLE */
    CLEAR_FLAG(img, 0x10);                                        /* not SCROLLABLE */
    ADD_EVENT_CB(img, (void *)on_event, 0, blk);                  /* LV_EVENT_ALL */
    MOTOR_ONCE(69, 0);
}
