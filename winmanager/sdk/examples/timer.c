/* Timer and stopwatch. Top tabs choose; timer presets 1/5/10/25 min; the watch buzzes when the time is up. Stopwatch keeps the last 5 laps. */
#include "f3game.h"

typedef struct { F3App base; int mode, preset, running, ring, ring_n; u32 start, base_ms, last_ring; int total, laps[5], nlaps; } St;   /* mode 0 timer, 1 stopwatch */
static const int PRE[4] = { 1, 5, 10, 25 };

static u32 elapsed(St *s) { return s->base_ms + (s->running ? A->ticks() - s->start : 0u); }
static void clock_text(char *b, u32 ms, int ds) {
    char t[12]; int m = (int)(ms / 60000u), sec = (int)(ms / 1000u % 60u), cs = (int)(ms / 10u % 100u), n = 0;
    itoa_pad(t, m, 2); for (int i = 0; t[i]; i++) b[n++] = t[i]; b[n++] = ':'; itoa_pad(t, sec, 2); for (int i = 0; t[i]; i++) b[n++] = t[i];
    if (ds) { b[n++] = '.'; itoa_pad(t, cs, 2); for (int i = 0; t[i]; i++) b[n++] = t[i]; } b[n] = 0;
}
static void set_timer(St *s, int k) { s->preset = k; s->total = PRE[k] * 60000; s->running = 0; s->base_ms = 0; s->ring = 0; }
static void btn(int x, int y, int w, int h, const char *t, u16 bg) { A->rrect(FB, x, y, w, h, 10, bg); A->ctext(FB, x + w / 2, y + (h - 12) / 2, t, 1, C_TEXT); }
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed;
    if (tap) {
        if (y < 40 && x < 216) { s->mode = x < 108 ? 0 : 1; s->running = 0; s->base_ms = 0; s->nlaps = 0; s->ring = 0; if (s->mode == 0) set_timer(s, s->preset); }
        else if (s->mode == 0 && y >= 64 && y < 104) { int k = x / 64; if (k >= 0 && k < 4) set_timer(s, k); }
        else if (y >= 300 && y < 350) {
            if (x < 128) { if (s->running) { s->base_ms += now - s->start; s->running = 0; } else { s->start = now; s->running = 1; s->ring = 0; } A->vibrate(1); }
            else if (s->mode == 0) { s->running = 0; s->base_ms = 0; s->ring = 0; }
            else if (s->running && s->nlaps < 5) { s->laps[s->nlaps++] = (int)elapsed(s); A->vibrate(1); } else if (!s->running) { s->base_ms = 0; s->nlaps = 0; }
        }
    }
    int shown = (int)elapsed(s);
    if (s->mode == 0 && s->running && shown >= s->total) { s->running = 0; s->base_ms = (u32)s->total; s->ring = 1; s->ring_n = 0; s->last_ring = now; A->vibrate(4); }
    if (s->ring && s->ring_n < 6 && now - s->last_ring > 700) { s->last_ring = now; s->ring_n++; A->vibrate(4); }
    clear(C_BG); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48));
    A->rrect(FB, 6, 4, 100, 26, 8, s->mode == 0 ? RGB(66, 87, 216) : RGB(44, 50, 84)); A->ctext(FB, 56, 10, "Timer", 1, C_TEXT);
    A->rrect(FB, 110, 4, 100, 26, 8, s->mode == 1 ? RGB(66, 87, 216) : RGB(44, 50, 84)); A->ctext(FB, 160, 10, TR("Cronômetro", "Stopwatch"), 1, C_TEXT);
    char buf[24];
    if (s->mode == 0) {
        for (int k = 0; k < 4; k++) { A->rrect(FB, 6 + k * 62, 64, 56, 38, 10, k == s->preset ? RGB(66, 87, 216) : RGB(44, 50, 84)); A->num(FB, 6 + k * 62 + (PRE[k] > 9 ? 12 : 20), 74, PRE[k], 2, C_TEXT); }
        int rem = s->total - shown; if (rem < 0) rem = 0; clock_text(buf, (u32)((rem + 999) / 1000) * 1000u, 0);
        A->ctext(FB, 128, 160, buf, 5, s->ring ? RGB(255, 120, 110) : C_TEXT);
        A->ctext(FB, 128, 240, s->ring ? TR("Acabou!", "Time is up!") : s->running ? TR("contando...", "counting...") : TR("escolha e inicie", "pick and start"), 1, C_MUTED);
    } else {
        clock_text(buf, (u32)shown, 1); A->ctext(FB, 128, 90, buf, 3, C_TEXT);
        for (int i = 0; i < s->nlaps; i++) { char t[12]; A->text(FB, 40, 150 + i * 28, "#", 2, C_MUTED); A->num(FB, 56, 150 + i * 28, i + 1, 2, C_MUTED); clock_text(t, (u32)s->laps[i], 1); A->text(FB, 100, 150 + i * 28, t, 2, C_TEXT); }
    }
    btn(10, 300, 112, 50, s->running ? TR("Pausar", "Pause") : TR("Iniciar", "Start"), s->running ? RGB(190, 120, 40) : RGB(40, 150, 90));
    btn(134, 300, 112, 50, s->mode == 1 && s->running ? TR("Volta", "Lap") : TR("Zerar", "Reset"), RGB(60, 70, 120));
    (void)now; return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; set_timer(s, 1); return &s->base;
}
