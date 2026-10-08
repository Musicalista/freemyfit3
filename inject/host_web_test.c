#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned fake_now = 5000;
static unsigned char fake_timer[64];
#define MOTOR_ONCE(a, b) (0)
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
/* in-memory VFS */
static unsigned char fs_data[2][32768]; static int fs_len[2] = { 0, 0 }; static int fs_pos[8]; static int fs_file[8]; static char fs_name[2][64] = { "/user/web_req.txt", "/user/web_page.txt" };
static int h_open(const char *p, const char *m) {
    for (int i = 0; i < 2; i++) if (!strcmp(p, fs_name[i])) { if (m[0] == 'w') fs_len[i] = 0; else if (!fs_len[i]) return -1; for (int f = 0; f < 8; f++) if (!fs_file[f]) { fs_file[f] = i + 1; fs_pos[f] = 0; return f + 3; } }
    return -1;
}
static int h_read(int fd, void *b, unsigned n) { int f = fd - 3, i = fs_file[f] - 1; int r = fs_len[i] - fs_pos[f]; if (r > (int)n) r = (int)n; if (r <= 0) return 0; memcpy(b, fs_data[i] + fs_pos[f], r); fs_pos[f] += r; return r; }
static int h_write(int fd, const void *b, unsigned n) { int f = fd - 3, i = fs_file[f] - 1; memcpy(fs_data[i] + fs_len[i], b, n); fs_len[i] += (int)n; return (int)n; }
static int h_close(int fd) { fs_file[fd - 3] = 0; return 0; }
#define FS_OPEN h_open
#define FS_READ h_read
#define FS_WRITE h_write
#define FS_CLOSE h_close
#include "fit3_apps.c"
static void dump(const u16 *fb, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n256 402\n255\n");
    for (int i = 0; i < 256 * 402; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
static void tick(WR *w, int n) { for (int i = 0; i < n; i++) { fake_now += 100; wr_tick(fake_timer); } }
static void tap(WR *w, int x, int y) { w->x = x; w->y = y; w->moved = 0; wr_tap(w, x, y); }
int main(void) {
    printf("sizeof(WR)=%zu\n", sizeof(WR));
    int ok = wr_open((void *)1, 0); WR *w = *(WR **)(fake_timer + 0xC); printf("wr_open=%d (help page: %d lines)\n", ok, w->nlines);
    dump(w->px, "web_help.ppm");
    /* the bridge wrote a page for seq 42; the watch asks for it */
    FILE *f = fopen("sample_page.txt", "rb"); fs_len[1] = (int)fread(fs_data[1], 1, 32768, f); fclose(f);
    tap(w, 80, 14);                                                     /* "Ir" opens the keyboard */
    printf("keyboard opened=%d\n", w->kbd_open);
    wr_kbd_done(w, "exemplo.com.br");                                    /* user typed an address and pressed OK */
    printf("request file: [%.*s] (len %d) mode=%d seq_wait=%u\n", fs_len[0] - 1, fs_data[0], fs_len[0], w->mode, w->seq_wait);
    dump(w->px, "web_wait.ppm"); tick(w, 1);
    printf("still waiting (seq mismatch): mode=%d\n", w->mode);
    /* bridge answers with the matching seq */
    { char tmp[40]; snprintf(tmp, sizeof tmp, "S%u", w->seq_wait); unsigned char *p = fs_data[1]; int sl = 1; while (p[2 + sl] != '\n') sl++; /* replace S42 with the awaited seq */
      unsigned char copy[32768]; int n = fs_len[1]; memcpy(copy, p, n); int tl = (int)strlen(tmp); memcpy(p + 3, tmp + 0, 0); int pos = 3; memmove(p + pos + tl, copy + 3 + 3, n - 6); memcpy(p + pos, tmp, tl); fs_len[1] = n - 3 + tl; }
    for (int i = 0; i < 12 && w->mode == WM_WAIT; i++) tick(w, 1);
    printf("page loaded: mode=%d title=[%s] url=[%s] lines=%d links=%d\n", w->mode, w->title, w->url, w->nlines, w->nlinks);
    tick(w, 1); dump(w->px, "web_page.ppm");
    w->top = 8; w->dirty = 1; tick(w, 1); dump(w->px, "web_scrolled.ppm");
    /* tap on the first "[n]" token visible */
    int found = 0, lx = 0, ly = 0, ln = 0;
    for (int y = WR_TOP; y < WR_BOT && !found; y += WR_LH) for (int x = 4; x < 252 && !found; x += 4) { int n = wr_link_at(w, x, y); if (n) { found = 1; lx = x; ly = y + 4; ln = n; } }
    printf("link token at (%d,%d) -> [%d]\n", lx, ly, ln);
    char want[200]; snprintf(want, sizeof want, "%s", (const char *)w->buf + w->link_off[ln]);
    tap(w, lx, ly); printf("tap -> request file: [%.*s] ; expected url=%s ; hist=%d mode=%d\n", fs_len[0] - 1, fs_data[0], want, w->nh, w->mode);
    return 0;
}
