/* Alarm clock: 3 alarms. Tap the hour or minute to change it (tap the top half to add, the bottom half to subtract), the switch turns it on.
 * The alarm rings (vibrates and flashes) while this app is open and the screen is on; tap to stop. Needs the v1.7 firmware (api->time). */
#include "f3game.h"

#define SAVE "/user/alarme.sav"
#define N 3
typedef struct { F3App base; int hh[N], mm[N], on[N], ring, fired_min, last_vib; u32 ring_t; } St;

static void save(St *s) { int fd = A->fopen(SAVE, "w"); if (fd < 0) return; int v[N * 3]; for (int i = 0; i < N; i++) { v[i * 3] = s->hh[i]; v[i * 3 + 1] = s->mm[i]; v[i * 3 + 2] = s->on[i]; } A->fwrite(fd, v, sizeof v); A->fclose(fd); }
static void load(St *s) {
    for (int i = 0; i < N; i++) { s->hh[i] = 7 + i; s->mm[i] = 0; s->on[i] = 0; }
    int fd = A->fopen(SAVE, "r"); if (fd < 0) return; int v[N * 3]; if (A->fread(fd, v, sizeof v) == (int)sizeof v) for (int i = 0; i < N; i++) { s->hh[i] = clampi(v[i * 3], 0, 23); s->mm[i] = clampi(v[i * 3 + 1], 0, 59); s->on[i] = v[i * 3 + 2] ? 1 : 0; } A->fclose(fd);
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; u32 now = A->ticks(); (void)pressed; F3Time t; int have = A->time != 0; if (have) A->time(&t);
    if (s->ring) {
        if (now - s->last_vib >= 600u) { s->last_vib = (int)now; A->vibrate(4); }
        if (tap || now - s->ring_t > 60000u) { s->ring = 0; }
    } else if (tap && have) {
        for (int i = 0; i < N; i++) {
            int top = 60 + i * 100;
            if (!in_rect(x, y, 0, top, 256, 92)) continue;
            if (in_rect(x, y, 6, top, 76, 92)) { s->hh[i] = (s->hh[i] + (y < top + 46 ? 1 : 23)) % 24; save(s); }
            else if (in_rect(x, y, 90, top, 76, 92)) { s->mm[i] = (s->mm[i] + (y < top + 46 ? 5 : 55)) % 60; save(s); }
            else if (x >= 176) { s->on[i] ^= 1; A->vibrate(1); save(s); }
        }
    }
    if (have && !s->ring) {
        int cur = t.hour * 60 + t.min;
        for (int i = 0; i < N; i++) if (s->on[i] && t.hour == s->hh[i] && t.min == s->mm[i] && t.sec < 30 && s->fired_min != cur) { s->ring = 1; s->ring_t = now; s->fired_min = cur; s->last_vib = (int)now - 1000; }
    }
    if (s->ring) { int flash = (now / 400u) & 1u; clear(flash ? RGB(200, 40, 40) : RGB(60, 10, 10)); A->ctext(FB, 128, 150, TR("ALARME", "ALARM"), 4, RGB(255, 255, 255)); A->ctext(FB, 128, 230, TR("toque para parar", "tap to stop"), 2, RGB(255, 230, 230)); return 1; }
    clear(C_BG); A->rect(FB, 0, 0, F3_W, 34, RGB(24, 28, 48)); A->text(FB, 8, 9, TR("Alarme", "Alarm"), 2, C_TEXT);
    if (!have) { A->ctext(FB, 128, 170, TR("Precisa do firmware", "Needs the firmware"), 2, C_TEXT); A->ctext(FB, 128, 200, TR("mais novo (v1.7)", "update (v1.7)"), 2, C_TEXT); return 1; }
    { char b[16]; itoa_pad(b, t.hour, 2); A->text(FB, 130, 8, b, 1, C_MUTED); A->text(FB, 146, 8, ":", 1, C_MUTED); itoa_pad(b, t.min, 2); A->text(FB, 154, 8, b, 1, C_MUTED); }
    for (int i = 0; i < N; i++) {
        int top = 60 + i * 100; A->rrect(FB, 4, top, 248, 92, 14, RGB(30, 34, 60));
        char b[8]; itoa_pad(b, s->hh[i], 2); A->text(FB, 16, top + 30, b, 4, s->on[i] ? C_TEXT : C_MUTED); A->text(FB, 80, top + 28, ":", 4, C_MUTED);
        itoa_pad(b, s->mm[i], 2); A->text(FB, 98, top + 30, b, 4, s->on[i] ? C_TEXT : C_MUTED);
        A->text(FB, 40, top + 6, "+", 2, C_MUTED); A->text(FB, 40, top + 74, "-", 1, C_MUTED); A->text(FB, 120, top + 6, "+", 2, C_MUTED); A->text(FB, 120, top + 74, "-", 1, C_MUTED);
        A->rrect(FB, 186, top + 28, 56, 32, 16, s->on[i] ? RGB(60, 190, 110) : RGB(70, 76, 110)); A->disc(FB, s->on[i] ? 226 : 202, top + 44, 12, RGB(255, 255, 255));
    }
    A->ctext(FB, 128, 366, TR("toca só com o app aberto", "rings only while the app is open"), 1, C_MUTED);
    return 1;
}
static void close_(F3App *self) { A->free(self); }
F3App *f3app_main(const F3Api *api) {
    A = api; St *s = (St *)api->malloc(sizeof(St)); if (!s) return 0;
    fill8(s, 0, sizeof(St)); s->base.tick = tick; s->base.close = close_; s->fired_min = -1; load(s); return &s->base;
}
