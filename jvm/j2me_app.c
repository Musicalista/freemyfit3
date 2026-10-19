/* j2me_app.c: Fit3 app that runs J2ME MIDlets (.jar). Jars live in /user/j2me/ with a list file:  /user/apps/jm_list.txt = "file.jar;Name;size" per line,
 * and the Java runtime in /user/apps/jm_rt.pak. The canvas is 240x320 shown at (8, 28); under it a pad sends key events (arrows, OK, soft keys, numbers). */
#include "f3app.h"
#include "sys.h"
#include "midp.h"
#undef malloc
#undef free
#undef calloc
#undef realloc
extern void jm_set_alloc(void *(*m)(uint32_t), void (*f)(void *));

#define CX0 8
#define CY0 28
#define MAXF 8
typedef struct {
    F3App base; Midp *m; int mode;                       /* 0 list, 1 loading, 2 running, 3 message */
    int nfiles; char file[MAXF][28]; char name[MAXF][22]; uint32_t size[MAXF]; int pick;
    int down, padkey, ptr, numpad, frames; uint32_t last_ms; char msg[96]; char log[96]; int rr;
} St;
static const F3Api *A;
static St *S;

/* ---------- host callbacks ---------- */
static void *hm(uint32_t n) { return A->malloc(n); }
static void hf(void *p) { A->free(p); }
static uint8_t *read_all(const char *path, uint32_t hint, uint32_t *len) {
    int fd = A->fopen(path, "r"); if (fd < 0) return 0;
    uint32_t cap = hint ? hint : 4096, n = 0; uint8_t *b = (uint8_t *)jm_alloc(cap + 8);
    while (b) {
        if (n == cap) { cap *= 2; uint8_t *nb = (uint8_t *)jm_realloc(b, cap + 8); if (!nb) { jm_free(b); b = 0; break; } b = nb; }
        int r = A->fread(fd, b + n, cap - n); if (r <= 0) break; n += (uint32_t)r;
    }
    A->fclose(fd); if (b && !n) { jm_free(b); return 0; }
    if (b) *len = n; return b;
}
static uint8_t *h_read(const char *path, uint32_t *len) { return read_all(path, 0, len); }
static int h_write(const char *path, const uint8_t *d, uint32_t n) {
    int fd = A->fopen(path, "w"); if (fd < 0) return -1;
    uint32_t o = 0; while (o < n) { int r = A->fwrite(fd, d + o, n - o > 2048 ? 2048 : n - o); if (r <= 0) break; o += (uint32_t)r; }
    A->fclose(fd); return o == n ? 0 : -1;
}
static int h_delete(const char *path) { int fd = A->fopen(path, "w"); if (fd < 0) return -1; A->fclose(fd); return 0; }
static void h_log(const char *s) { int i = 0; for (; s[i] && i < 94; i++) S->log[i] = s[i] >= 32 && s[i] < 127 ? s[i] : ' '; S->log[i] = 0; }
static void h_vib(int ms) { A->vibrate(ms >= 100 ? 4 : 1); }
static uint32_t h_ticks(void) { return A->ticks() + 1; }

/* ---------- list ---------- */
static void parse_list(St *s) {
    uint32_t n = 0; uint8_t *b = read_all("/user/apps/jm_list.txt", 0, &n); s->nfiles = 0; if (!b) return;
    uint32_t i = 0;
    while (i < n && s->nfiles < MAXF) {
        char *f[3] = { (char *)b + i, 0, 0 }; int nf = 1;
        while (i < n && b[i] != '\n') { if (b[i] == ';' && nf < 3) { b[i] = 0; f[nf++] = (char *)b + i + 1; } else if (b[i] == '\r') b[i] = 0; i++; }
        if (i < n) b[i++] = 0; else b[n - 1] = b[n - 1];
        if (nf < 2 || !f[0][0]) continue;
        int k = s->nfiles, j;
        for (j = 0; f[0][j] && j < 27; j++) s->file[k][j] = f[0][j]; s->file[k][j] = 0;
        for (j = 0; f[1][j] && j < 21; j++) s->name[k][j] = f[1][j]; s->name[k][j] = 0;
        s->size[k] = 0; if (nf >= 3) for (j = 0; f[2][j] >= '0' && f[2][j] <= '9'; j++) s->size[k] = s->size[k] * 10 + (uint32_t)(f[2][j] - '0');
        s->nfiles++;
    }
    jm_free(b);
}
static void msg(St *s, const char *t) { int i = 0; for (; t[i] && i < 94; i++) s->msg[i] = t[i]; s->msg[i] = 0; s->mode = 3; }
static void draw_list(St *s) {
    A->rect(A->fb, 0, 0, F3_W, F3_H, RGB(14, 16, 28)); A->text(A->fb, 8, 9, "J2ME", 2, RGB(236, 240, 250));
    if (!s->nfiles) { A->text(A->fb, 8, 60, A->lang_pt ? "Nenhum jogo instalado." : "No games installed.", 1, RGB(236, 240, 250)); A->text(A->fb, 8, 80, A->lang_pt ? "Use o Fit3 Manager (J2ME)." : "Use Fit3 Manager (J2ME).", 1, RGB(140, 150, 175)); return; }
    for (int i = 0; i < s->nfiles; i++) { int y = 44 + i * 40; A->rrect(A->fb, 8, y, 240, 34, 10, RGB(34, 40, 66)); A->text(A->fb, 20, y + 11, s->name[i], 2, RGB(236, 240, 250)); }
}

/* ---------- running ---------- */
static const char *PADL[8] = { "<", "^", "v", ">", "OK", "L", "R", "123" };
static const int PADK[8] = { -3, -1, -2, -4, -5, -6, -7, 0 };
static const char NUML[12] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#' };
static const int NUMK[12] = { 49, 50, 51, 52, 53, 54, 55, 56, 57, 42, 48, 35 };
static int pad_hit(St *s, int x, int y) {                 /* returns index of the pad button under (x,y), or -1 */
    if (y < 352) return -1;
    if (!s->numpad) { int i = (x - 2) / 31; return i >= 0 && i < 8 ? i : -1; }
    int r = (y - 352) / 25, c = (x - 4) / 41; if (r > 1 || c < 0 || c > 5) return -1; return r * 6 + c;
}
static void draw_pad(St *s) {
    A->rect(A->fb, 0, 350, F3_W, 52, RGB(20, 22, 36));
    if (!s->numpad) for (int i = 0; i < 8; i++) { A->rrect(A->fb, 3 + i * 31, 356, 29, 40, 8, i == 4 ? RGB(60, 110, 200) : RGB(44, 50, 78)); A->ctext(A->fb, 17 + i * 31, 369, PADL[i], i == 7 ? 1 : 2, RGB(236, 240, 250)); }
    else { for (int i = 0; i < 12; i++) { int r = i / 6, c = i % 6; char t[2] = { NUML[i], 0 }; A->rrect(A->fb, 4 + c * 41, 353 + r * 25, 38, 22, 6, RGB(44, 50, 78)); A->ctext(A->fb, 23 + c * 41, 357 + r * 25, t, 1, RGB(236, 240, 250)); }
           A->rrect(A->fb, 4 + 5 * 41 - 0, 353 + 25, 38, 22, 6, RGB(60, 110, 200)); A->ctext(A->fb, 23 + 5 * 41, 357 + 25, "abc", 1, RGB(236, 240, 250)); }
}
static void blit(St *s) {
    int dirty; const uint16_t *src = midp_screen(s->m, &dirty); if (!dirty) return;
    for (int y = 0; y < MIDP_H_; y++) memcpy(A->fb + (CY0 + y) * F3_W + CX0, src + y * MIDP_W, MIDP_W * 2);
    s->frames++;
}
static int start_game(St *s) {
    char path[48]; int n = 0; const char *p = "/user/apps/"; while (*p) path[n++] = *p++; for (int j = 0; s->file[s->pick][j]; j++) path[n++] = s->file[s->pick][j]; path[n] = 0;
    uint32_t jl = 0, rl = 0; uint8_t *jar = read_all(path, s->size[s->pick], &jl), *rt = jar ? read_all("/user/apps/jm_rt.pak", 0, &rl) : 0;
    if (!jar) { msg(s, A->lang_pt ? "Nao achei o .jar" : "Jar file not found"); return -1; }
    if (!rt) { jm_free(jar); msg(s, A->lang_pt ? "Falta rt.pak" : "rt.pak missing"); return -1; }
    char id[12]; int q = 0; for (int j = 0; s->file[s->pick][j] && s->file[s->pick][j] != '.' && q < 8; j++) { char c = s->file[s->pick][j]; id[q++] = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ? c : '_'; } id[q] = 0;
    MidpHost h = { h_read, h_write, h_delete, h_log, h_vib, h_ticks };
    static const uint32_t heaps[] = { 640, 512, 384, 256, 192, 128 };
    for (unsigned k = 0; k < sizeof heaps / sizeof heaps[0]; k++) { s->m = midp_create(&h, jar, jl, rt, rl, id, heaps[k] * 1024); if (s->m) break; }
    if (!s->m) { jm_free(jar); jm_free(rt); msg(s, A->lang_pt ? "Sem memoria" : "Out of memory"); return -1; }
    int r = midp_boot(s->m);
    if (r) { const char *e = midp_error(s->m); msg(s, e ? e : "boot failed"); midp_destroy(s->m); s->m = 0; return -1; }
    A->rect(A->fb, 0, 0, F3_W, F3_H, RGB(0, 0, 0)); draw_pad(s); s->mode = 2; s->last_ms = A->ticks(); return 0;
}
static int tick(F3App *self, int pressed, int x, int y, int tap) {
    St *s = (St *)self; (void)tap;
    if (s->mode == 0) {
        draw_list(s);
        if (pressed && !s->down) { s->down = 1; for (int i = 0; i < s->nfiles; i++) if (y >= 44 + i * 40 && y < 78 + i * 40) { s->pick = i; s->mode = 1; A->rect(A->fb, 0, 0, F3_W, F3_H, RGB(14, 16, 28)); A->text(A->fb, 20, 180, A->lang_pt ? "Carregando..." : "Loading...", 2, RGB(236, 240, 250)); } }
        if (!pressed) s->down = 0;
        return 1;
    }
    if (s->mode == 1) { if (start_game(s) == 0) return 1; return 1; }
    if (s->mode == 3) {
        A->rect(A->fb, 0, 0, F3_W, F3_H, RGB(14, 16, 28)); A->text(A->fb, 8, 60, A->lang_pt ? "Parou:" : "Stopped:", 2, RGB(255, 180, 120));
        { int i = 0, yy = 100; const char *t = s->msg; char line[30]; while (t[i]) { int k = 0; while (t[i] && k < 29) line[k++] = t[i++]; line[k] = 0; A->text(A->fb, 8, yy, line, 1, RGB(236, 240, 250)); yy += 14; } }
        if (pressed && tap) return 0; return 1;
    }
    /* running */
    Midp *m = s->m;
    if (pressed) {
        int px = x - CX0, py = y - CY0;
        if (!s->down) {
            s->down = 1; int b = pad_hit(s, x, y);
            if (b >= 0) { s->ptr = 0;
                if (!s->numpad && b == 7) { s->numpad = 1; draw_pad(s); s->padkey = 0; }
                else if (s->numpad && b == 11) { s->numpad = 0; draw_pad(s); s->padkey = 0; }
                else { s->padkey = s->numpad ? NUMK[b] : PADK[b]; midp_event(m, 1, s->padkey); } }
            else if (px >= 0 && px < MIDP_W && py >= 0 && py < MIDP_H_) { s->ptr = 1; midp_event(m, 3, px | (py << 16)); }
        } else if (s->ptr && px >= 0 && px < MIDP_W && py >= 0 && py < MIDP_H_) midp_event(m, 4, px | (py << 16));
    } else if (s->down) {
        s->down = 0;
        if (s->padkey) { midp_event(m, 2, s->padkey); s->padkey = 0; }
        if (s->ptr) { midp_event(m, 5, 0); s->ptr = 0; }
    }
    midp_run(m, 22);
    if (midp_finished(m)) { const char *e = midp_error(m); msg(s, e ? e : (A->lang_pt ? "O jogo terminou." : "The MIDlet ended.")); midp_destroy(m); s->m = 0; return 1; }
    blit(s);
    return 1;
}
static void close_(F3App *self) { St *s = (St *)self; if (s->m) midp_destroy(s->m); jm_free(s); }
F3App *f3app_main(const F3Api *api) {
    A = api; jm_set_alloc(hm, hf);
    St *s = (St *)jm_alloc(sizeof(St)); if (!s) return 0; S = s;
    s->base.tick = tick; s->base.close = close_; parse_list(s); s->mode = 0; return &s->base;
}
