/* Fit3 app SDK: tiny native apps that the watch loads from /user/apps/<id>.f3a (installed from the PC with Fit3 Manager, no reflashing).
 * An app is plain freestanding C compiled for Cortex-M4 (Thumb) with apps/appcc.py; globals are allowed (they live in the app's own RAM image).
 * The watch calls f3app_main() once, then app->tick() every ~33 ms. Draw the whole frame into api->fb (256x402 RGB565); the watch shows it and draws the X button.
 * No libc: use the functions in F3Api. Keep apps small (a few KB) and never block: return from tick() quickly. */
#ifndef F3APP_H
#define F3APP_H
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#define F3_API_VERSION 1
#define F3_W 256
#define F3_H 402
typedef struct { int year, month, day, hour, min, sec, wday; } F3Time;      /* real time from the watch's RTC (year e.g. 2026, month 1-12, wday as the firmware reports it) */
typedef struct F3Api {
    u32 version; u16 *fb; int w, h;                                    /* canvas, RGB565, row stride = w */
    void *(*malloc)(u32 n); void (*free)(void *p);
    u32 (*ticks)(void);                                                /* milliseconds */
    void (*vibrate)(int id);                                           /* 1 short, 4 long, 5 error */
    void (*rect)(u16 *fb, int x, int y, int w, int h, u16 color);
    void (*rrect)(u16 *fb, int x, int y, int w, int h, int radius, u16 color);
    void (*disc)(u16 *fb, int cx, int cy, int r, u16 color);
    void (*text)(u16 *fb, int x, int y, const char *s, int scale, u16 color);     /* ASCII, 8x12 font x scale */
    void (*ctext)(u16 *fb, int cx, int y, const char *s, int scale, u16 color);   /* centred on cx */
    void (*num)(u16 *fb, int x, int y, int value, int scale, u16 color);
    int (*fopen)(const char *path, const char *mode);                  /* the watch's own file system: "r" / "w"; keep to /user/ */
    int (*fread)(int fd, void *buf, u32 n); int (*fwrite)(int fd, const void *buf, u32 n); int (*fclose)(int fd);
    u32 lang_pt;                                                       /* 1 when the watch language is Portuguese */
    void (*time)(F3Time *t);                                           /* current date and time; NULL on firmware older than v1.7 (check before use) */
    int (*battery)(void);                                              /* battery percent 0-100; NULL on older firmware */
    u32 reserved[6];
} F3Api;
typedef struct F3App {
    int (*tick)(struct F3App *self, int pressed, int x, int y, int tap);   /* return 1 to keep running, 0 to exit; tap = 1 on the frame a tap was released */
    void (*close)(struct F3App *self);                                     /* optional; free your memory here */
} F3App;
F3App *f3app_main(const F3Api *api);                                       /* entry point: allocate your state (api->malloc) and return it, or 0 to fail */
#define RGB(r, g, b) ((u16)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#endif
