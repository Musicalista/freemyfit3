/* Probe 3: draw a 64x64 RGB565 test image via LVGL when the user CLICKS an item on the Settings>Vibration page.
 * Hook: bl lv_event_get_code(e) at 0x2c1b4b26 (start of the page's click handler). Quadrants: TL red, TR green, BL blue, BR white.
 * Result buzz: 69 = image created, 1 = no page, 5 = malloc failed. */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#define MOTOR_ONCE ((int (*)(int, int))0x2c1423f9)
#define EVENT_GET_CODE ((int (*)(void *))0x2c27d989)
#define MALLOC ((void *(*)(u32))0x2c0eb00d)
#define IMG_CREATE ((void *(*)(void *))0x2c2905ad)
#define IMG_SET_SRC ((void (*)(void *, const void *))0x2c29064d)
#define OBJ_ALIGN_TO ((void (*)(void *, void *, int, int, int))0x2c27908d)
#define W 64
#define H 64
void probe(void *ev) {
    if (EVENT_GET_CODE(ev) != 7) return;                      /* LV_EVENT_CLICKED */
    void **pg = *(void ***)0x20115f3c; if (!pg) { MOTOR_ONCE(1, 0); return; }
    void *root = pg[0]; if (!root) { MOTOR_ONCE(1, 0); return; }
    u32 *blk = (u32 *)MALLOC(16 + W * H * 2); if (!blk) { MOTOR_ONCE(5, 0); return; }
    u16 *px = (u16 *)((u8 *)blk + 16);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        u16 c = (y < H / 2) ? (x < W / 2 ? 0xF800 : 0x07E0) : (x < W / 2 ? 0x001F : 0xFFFF);
        if (x == 0 || y == 0 || x == W - 1 || y == H - 1) c = 0x0000;
        px[y * W + x] = c;
    }
    blk[0] = 4u | ((u32)W << 10) | ((u32)H << 21);              /* lv_img_dsc_t.header: cf=TRUE_COLOR, w, h */
    blk[1] = W * H * 2; blk[2] = (u32)px;
    void *img = IMG_CREATE(root);
    IMG_SET_SRC(img, blk);
    OBJ_ALIGN_TO(img, 0, 9, 0, 0);                             /* LV_ALIGN_CENTER */
    MOTOR_ONCE(69, 0);
}
