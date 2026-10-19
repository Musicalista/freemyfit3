/* Clock: real time and date from the watch's RTC, plus the battery level. Tap to change the colour theme. Needs the v1.7 firmware (api->time). */
#include "f3game.h"

#define SAVE "/user/relogio.sav"
typedef struct { F3App base; int theme, last_sec; } St;
static const u16 TH_BG[4] = { RGB(8, 10, 20), RGB(0, 0, 0), RGB(20, 8, 36), RGB(6, 26, 30) };
static const u16 TH_FG[4] = { RGB(240, 244, 255), RGB(70, 255, 120), RGB(255, 180, 240), RGB(130, 235, 235) };
static const u16 TH_AC[4] = { RGB(120, 150, 255), RGB(30, 150, 70), RGB(190, 110, 220), RGB(60, 150, 160) };
static const char *const DOW_PT[7] = { "domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado" };
static const char *const DOW_EN[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

static int weekday(int y, int m, int d) {                        /* 0 = Sunday (does not depend on how the firmware numbers days) */
    static const int T[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 }; if (m < 3) y--; return (y + y / 4 - y / 100 + y / 400 + T[m - 1] + d) % 7;
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)pressed; (void)x; (void)y;
    if (tap) { s->theme = (s->theme + 1) & 3; A->vibrate(1); hs_save(SAVE, s->theme); }
    u16 bg = TH_BG[s->theme], fg = TH_FG[s->theme], ac = TH_AC[s->theme];
    clear(bg);
    if (!A->time) { A->ctext(FB, 128, 170, TR("Precisa do firmware", "Needs the firmware"), 2, fg); A->ctext(FB, 128, 200, TR("mais novo (v1.7)", "update (v1.7)"), 2, fg); return 1; }
    F3Time t; A->time(&t); char b[16];
    int dw = (t.month >= 1 && t.month <= 12) ? weekday(t.year, t.month, t.day) : 0;
    itoa_pad(b, t.hour, 2); A->text(FB, 8, 110, b, 7, fg);
    A->rect(FB, 118, 128, 8, 8, (t.sec & 1) ? ac : bg); A->rect(FB, 118, 160, 8, 8, (t.sec & 1) ? ac : bg);
    itoa_pad(b, t.min, 2); A->text(FB, 136, 110, b, 7, fg);
    itoa_pad(b, t.sec, 2); A->ctext(FB, 128, 214, b, 3, ac);
    A->ctext(FB, 128, 262, TR(DOW_PT[dw], DOW_EN[dw]), 2, fg);
    { char d[20]; int n = 0; char t2[12]; itoa_pad(t2, t.day, 2); for (int i = 0; t2[i]; i++) d[n++] = t2[i]; d[n++] = '/'; itoa_pad(t2, t.month, 2); for (int i = 0; t2[i]; i++) d[n++] = t2[i]; d[n++] = '/'; itoa_pad(t2, t.year, 4); for (int i = 0; t2[i]; i++) d[n++] = t2[i]; d[n] = 0; A->ctext(FB, 128, 290, d, 2, ac); }
    if (A->battery) {
        int pct = A->battery(); u16 bc = pct > 40 ? RGB(80, 220, 120) : pct > 15 ? RGB(240, 190, 60) : RGB(240, 80, 70);
        A->rrect(FB, 78, 352, 90, 24, 6, ac); A->rrect(FB, 80, 354, 86, 20, 5, bg); A->rect(FB, 168, 360, 5, 8, ac); A->rrect(FB, 82, 356, 82 * pct / 100, 16, 4, bc);
        char p[12]; itoa_pad(p, pct, 1); int n = 0; while (p[n]) n++; p[n++] = '%'; p[n] = 0; A->ctext(FB, 128, 332, p, 1, fg);
    }
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->theme = hs_load(SAVE) & 3; return &s->base;
}
