/* midp.c: native side of the Java runtime (java.lang natives, lcdui Graphics/Image, RMS, events) for the Fit3 J2ME player. See midp.h. */
#include "sys.h"
#include "vm.h"
#include "jar.h"
#include "png.h"
#include "midp.h"
#include "jmath.h"
#include "kbd_font.h"

#define EVQ 64
struct Midp {
    MidpHost h; VM vm; Jar jar, rt; uint8_t *jarbuf, *rtbuf; char id[24]; char midlet[96];
    uint16_t *scr; int dirty; int quit, finished; const char *err; char errbuf[160];
    int ev[EVQ][2]; int evr, evw, last_arg;
    uint32_t ctab; uint32_t t0;
    struct { int img, color, tx, ty, cx, cy, cw, ch, font; } gs;
    struct { int w, h, px, alpha; } is;
    int fs_size; int cls_id_slot;
    int die_logged;
};
static Midp *MM(VM *vm) { return (Midp *)vm->user; }
#define HW(vm, r) ((uint32_t *)((vm)->heap + (r)))
#define FLD(vm, obj, slot) (((int32_t *)((vm)->heap + (obj) + 8)) + (slot))

/* ---------- small helpers ---------- */
static uint32_t new_arr(VM *vm, int type, int32_t n) { uint32_t r = vm_new_array(vm, type, n); if (!r) { vm_gc(vm); r = vm_new_array(vm, type, n); } return r; }
static uint32_t new_str(VM *vm, const char *s) { uint32_t r = vm_new_string(vm, s); if (!r) { vm_gc(vm); r = vm_new_string(vm, s); } return r; }
static const uint16_t *jchars(VM *vm, uint32_t s, int *n) {                   /* chars of a java.lang.String (slot 0 = char[]) */
    uint32_t arr = (uint32_t)*FLD(vm, s, 0);
    if (!arr) { *n = 0; return 0; }
    *n = (int)HW(vm, arr)[1]; return (const uint16_t *)(vm->heap + arr + 8);
}
static void jtoc(VM *vm, uint32_t s, char *out, int cap) {
    int n = 0, i; const uint16_t *c = s ? jchars(vm, s, &n) : 0;
    for (i = 0; i < n && i < cap - 1; i++) out[i] = (char)(c[i] < 256 ? c[i] : '?');
    out[i] = 0;
}
static int64_t arg64(int32_t *a) { return (int64_t)(((uint64_t)(uint32_t)a[1] << 32) | (uint32_t)a[0]); }
static double argd(int32_t *a) { int64_t v = arg64(a); double d; memcpy(&d, &v, 8); return d; }
static void ret64(VM *vm, int64_t v) { vm->ret[0] = (int32_t)v; vm->ret[1] = (int32_t)(v >> 32); }
static void retd(VM *vm, double d) { int64_t v; memcpy(&v, &d, 8); ret64(vm, v); }
static int esize(uint32_t t) { t &= 0xff; switch (t) { case 4: case 8: return 1; case 5: case 9: return 2; case 11: case 7: return 8; default: return 4; } }

/* ---------- java.lang.Object ---------- */
static uint32_t class_obj(VM *vm, Class *c) {
    Midp *m = MM(vm); int id = vm_class_id(c);
    if (!m->ctab) { m->ctab = new_arr(vm, 0, 1024); if (!m->ctab) return 0; }
    if (id >= 1024) return 0;
    uint32_t *tab = (uint32_t *)(vm->heap + m->ctab + 8);
    if (!tab[id]) {
        Class *cc = vm_find_class(vm, "java/lang/Class"); if (!cc) return 0;
        uint32_t o = vm_new_object(vm, cc); if (!o) { vm_gc(vm); o = vm_new_object(vm, cc); } if (!o) return 0;
        tab = (uint32_t *)(vm->heap + m->ctab + 8); *FLD(vm, o, 0) = id; tab[id] = o;
    }
    return tab[id];
}
static int n_getclass(VM *vm, int32_t *a) { uint32_t o = class_obj(vm, vm_obj_class(vm, (uint32_t)a[0])); if (!o) return vm_throw(vm, "java/lang/OutOfMemoryError"); vm->ret[0] = (int32_t)o; return 0; }
static int n_obj_tostring(VM *vm, int32_t *a) {
    char buf[160]; const char *n = vm_class_name(vm_obj_class(vm, (uint32_t)a[0])); int i = 0;
    for (; n[i] && i < 120; i++) buf[i] = n[i] == '/' ? '.' : n[i];
    buf[i++] = '@'; { uint32_t v = (uint32_t)a[0]; for (int k = 7; k >= 0; k--) { int d = (v >> (k * 4)) & 15; if (d || k < 3 || i > 0) buf[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); } } buf[i] = 0;
    uint32_t s = new_str(vm, buf); if (!s) return vm_throw(vm, "java/lang/OutOfMemoryError"); vm->ret[0] = (int32_t)s; return 0;
}
static int n_wait0(VM *vm, int32_t *a) { vm_wait(vm, (uint32_t)a[0], 0); return 0; }
static int n_wait1(VM *vm, int32_t *a) { int64_t ms = arg64(a + 1); uint32_t w = ms > 0 ? vm->now() + (uint32_t)ms : 0; if (ms > 0 && !w) w = 1; vm_wait(vm, (uint32_t)a[0], w); return 0; }
static int n_notify(VM *vm, int32_t *a) { vm_notify(vm, (uint32_t)a[0], 0); return 0; }
static int n_notifyall(VM *vm, int32_t *a) { vm_notify(vm, (uint32_t)a[0], 1); return 0; }

/* ---------- java.lang.String (value = slot 0, hash = slot 1) ---------- */
static int n_str_length(VM *vm, int32_t *a) { int n; jchars(vm, (uint32_t)a[0], &n); vm->ret[0] = n; return 0; }
static int n_str_charat(VM *vm, int32_t *a) { int n; const uint16_t *c = jchars(vm, (uint32_t)a[0], &n); if ((uint32_t)a[1] >= (uint32_t)n) return -2; vm->ret[0] = c[a[1]]; return 0; }
static int n_str_equals(VM *vm, int32_t *a) {
    uint32_t o = (uint32_t)a[1]; vm->ret[0] = 0;
    if (a[0] == a[1]) { vm->ret[0] = 1; return 0; }
    if (!o || vm_obj_class(vm, o) != vm_obj_class(vm, (uint32_t)a[0])) return 0;
    int nx, ny; const uint16_t *x = jchars(vm, (uint32_t)a[0], &nx), *y = jchars(vm, o, &ny);
    if (nx != ny) return 0;
    vm->ret[0] = !nx || !memcmp(x, y, (size_t)nx * 2); return 0;
}
static int n_str_hash(VM *vm, int32_t *a) {
    int32_t *h = FLD(vm, (uint32_t)a[0], 1); if (*h) { vm->ret[0] = *h; return 0; }
    int n; const uint16_t *c = jchars(vm, (uint32_t)a[0], &n); uint32_t v = 0; for (int i = 0; i < n; i++) v = v * 31 + c[i];
    *h = (int32_t)v; vm->ret[0] = (int32_t)v; return 0;
}
static int n_str_compare(VM *vm, int32_t *a) {
    if (!a[1]) return vm_throw(vm, "java/lang/NullPointerException");
    int nx, ny; const uint16_t *x = jchars(vm, (uint32_t)a[0], &nx), *y = jchars(vm, (uint32_t)a[1], &ny); int n = nx < ny ? nx : ny;
    for (int i = 0; i < n; i++) if (x[i] != y[i]) { vm->ret[0] = x[i] - y[i]; return 0; }
    vm->ret[0] = nx - ny; return 0;
}
static int n_str_indexc(VM *vm, int32_t *a) {
    int n; const uint16_t *c = jchars(vm, (uint32_t)a[0], &n); int from = a[2] < 0 ? 0 : a[2];
    for (int i = from; i < n; i++) if (c[i] == (uint32_t)a[1]) { vm->ret[0] = i; return 0; }
    vm->ret[0] = -1; return 0;
}
static int n_str_indexs(VM *vm, int32_t *a) {
    if (!a[1]) return vm_throw(vm, "java/lang/NullPointerException");
    int n, m; const uint16_t *c = jchars(vm, (uint32_t)a[0], &n), *t = jchars(vm, (uint32_t)a[1], &m); int from = a[2] < 0 ? 0 : a[2];
    for (int i = from; i + m <= n; i++) { int j = 0; while (j < m && c[i + j] == t[j]) j++; if (j == m) { vm->ret[0] = i; return 0; } }
    vm->ret[0] = -1; return 0;
}

/* ---------- java.lang.System / Runtime / Thread / Class ---------- */
static int n_arraycopy(VM *vm, int32_t *a) {
    uint32_t s = (uint32_t)a[0], d = (uint32_t)a[2]; int sp = a[1], dp = a[3], n = a[4];
    if (!s || !d) return vm_throw(vm, "java/lang/NullPointerException");
    uint32_t *hs = HW(vm, s), *hd = HW(vm, d);
    if (!(hs[0] & 0x80000000u) || !(hd[0] & 0x80000000u) || hs[0] != hd[0]) return vm_throw(vm, "java/lang/ArrayStoreException");
    if (sp < 0 || dp < 0 || n < 0 || (uint32_t)sp + (uint32_t)n > hs[1] || (uint32_t)dp + (uint32_t)n > hd[1]) return vm_throw(vm, "java/lang/ArrayIndexOutOfBoundsException");
    int es = esize(hs[0]); memmove(vm->heap + d + 8 + dp * es, vm->heap + s + 8 + sp * es, (size_t)n * (size_t)es); return 0;
}
static int n_millis(VM *vm, int32_t *a) { (void)a; ret64(vm, 1790000000000LL + (int64_t)(vm->now() - MM(vm)->t0)); return 0; }
static int n_idhash(VM *vm, int32_t *a) { vm->ret[0] = a[0]; return 0; }
static int n_gc(VM *vm, int32_t *a) { (void)a; vm_gc(vm); return 0; }
static int n_freemem(VM *vm, int32_t *a) { (void)a; ret64(vm, (int64_t)(vm->heap_size - vm->used)); return 0; }
static int n_totalmem(VM *vm, int32_t *a) { (void)a; ret64(vm, (int64_t)vm->heap_size); return 0; }
static int n_spawn0(VM *vm, int32_t *a) { vm->ret[0] = vm_spawn(vm, (uint32_t)a[0]) >= 0; return 0; }
static int n_isalive(VM *vm, int32_t *a) { vm->ret[0] = vm_thread_alive(vm, (uint32_t)a[0]); return 0; }
static int n_curthread(VM *vm, int32_t *a) { (void)a; vm->ret[0] = vm->cur ? (int32_t)vm->cur->obj : 0; return 0; }
static int n_sleep(VM *vm, int32_t *a) {
    int64_t ms = arg64(a); if (ms < 0) return vm_throw(vm, "java/lang/IllegalArgumentException");
    uint32_t w = vm->now() + (uint32_t)(ms > 100000000 ? 100000000 : ms); if (!w) w = 1;
    if (ms <= 0) vm_yield(vm, 0); else vm_yield(vm, w);
    return 0;
}
static int n_yield(VM *vm, int32_t *a) { (void)a; vm_yield(vm, 0); return 0; }
static int n_class_name(VM *vm, int32_t *a) {
    int id = *FLD(vm, (uint32_t)a[0], 0); char buf[160]; const char *n = vm_class_name(vm->classes[id]); int i = 0;
    for (; n[i] && i < 150; i++) buf[i] = n[i] == '/' ? '.' : n[i];
    buf[i] = 0; uint32_t s = new_str(vm, buf); if (!s) return vm_throw(vm, "java/lang/OutOfMemoryError"); vm->ret[0] = (int32_t)s; return 0;
}
static int n_forname(VM *vm, int32_t *a) {
    char buf[160]; jtoc(vm, (uint32_t)a[0], buf, sizeof buf); for (char *p = buf; *p; p++) if (*p == '.') *p = '/';
    Class *c = vm_find_class(vm, buf); if (!c) return vm_throw(vm, "java/lang/ClassNotFoundException");
    uint32_t o = class_obj(vm, c); if (!o) return vm_throw(vm, "java/lang/OutOfMemoryError"); vm->ret[0] = (int32_t)o; return 0;
}
static int n_newinstance(VM *vm, int32_t *a) {
    int id = *FLD(vm, (uint32_t)a[0], 0); Class *c = vm->classes[id];
    if (vm_init_class(vm, c)) return 1;
    uint32_t o = vm_new_object(vm, c); if (!o) { vm_gc(vm); o = vm_new_object(vm, c); } if (!o) return vm_throw(vm, "java/lang/OutOfMemoryError");
    Method *init = vm_find_method(c, "<init>", "()V"); if (!init) return vm_throw(vm, "java/lang/InstantiationException");
    int32_t arg = (int32_t)o; int r = vm_call(vm, init, &arg, 1); if (r) return r;
    vm->ret[0] = (int32_t)o; return 0;
}
static int n_f2i(VM *vm, int32_t *a) { vm->ret[0] = a[0]; return 0; }
static int n_d2l(VM *vm, int32_t *a) { vm->ret[0] = a[0]; vm->ret[1] = a[1]; return 0; }
#define MATH1(nm, fn) static int nm(VM *vm, int32_t *a) { retd(vm, fn(argd(a))); return 0; }
MATH1(n_sin, jm_sin) MATH1(n_cos, jm_cos) MATH1(n_sqrt, jm_sqrt) MATH1(n_floor, jm_floor) MATH1(n_ceil, jm_ceil) MATH1(n_atan, jm_atan)

/* ---------- jvmhost.Host ---------- */
static int n_res(VM *vm, int32_t *a) {
    Midp *m = MM(vm); char nm[200]; jtoc(vm, (uint32_t)a[0], nm, sizeof nm);
    const char *p = nm[0] == '/' ? nm + 1 : nm; uint32_t len;
    uint8_t *b = jar_read(&m->jar, p, &len);
    if (!b) { vm->ret[0] = 0; return 0; }
    uint32_t r = new_arr(vm, 8, (int32_t)len); if (!r) { free(b); return vm_throw(vm, "java/lang/OutOfMemoryError"); }
    memcpy(vm->heap + r + 8, b, len); free(b); vm->ret[0] = (int32_t)r; return 0;
}
static int n_pollevent(VM *vm, int32_t *a) {
    Midp *m = MM(vm); (void)a;
    if (m->evr == m->evw) { vm->ret[0] = 0; return 0; }
    vm->ret[0] = m->ev[m->evr][0]; m->last_arg = m->ev[m->evr][1]; m->evr = (m->evr + 1) % EVQ; return 0;
}
static int n_pollarg(VM *vm, int32_t *a) { (void)a; vm->ret[0] = MM(vm)->last_arg; return 0; }
static int n_width(VM *vm, int32_t *a) { (void)a; vm->ret[0] = MIDP_W; return 0; }
static int n_height(VM *vm, int32_t *a) { (void)a; vm->ret[0] = MIDP_H_; return 0; }
static int n_present(VM *vm, int32_t *a) {
    Midp *m = MM(vm);
    if (a[0]) {
        int32_t *f = FLD(vm, (uint32_t)a[0], 0); int w = f[m->is.w], h = f[m->is.h]; uint32_t pa = (uint32_t)f[m->is.px];
        if (pa) { const uint16_t *src = (const uint16_t *)(vm->heap + pa + 8); int cw = w < MIDP_W ? w : MIDP_W, ch = h < MIDP_H_ ? h : MIDP_H_;
            for (int y = 0; y < ch; y++) memcpy(m->scr + y * MIDP_W, src + y * w, (size_t)cw * 2); }
    }
    m->dirty = 1; return 0;
}
static int n_trace(VM *vm, int32_t *a) {
    char b[600]; int q = 0; (void)a;
    for (int i = 0; i < vm->nlt && q < 500; i++) { const char *cn = vm_method_class(vm->lt_m[i]), *mn = vm_method_name(vm->lt_m[i]); b[q++] = 10; b[q++] = ' '; b[q++] = ' ';
        while (*cn && q < 540) { b[q++] = *cn == '/' ? '.' : *cn; cn++; } b[q++] = '.'; while (*mn && q < 570) b[q++] = *mn++; b[q++] = '@'; { int pc = vm->lt_pc[i], d[8], nd = 0; if (!pc) d[nd++] = 0; while (pc) { d[nd++] = pc % 10; pc /= 10; } while (nd) b[q++] = (char)('0' + d[--nd]); } }
    b[q] = 0; uint32_t s = new_str(vm, b); vm->ret[0] = (int32_t)s; return 0;
}
static int n_log(VM *vm, int32_t *a) { char b[200]; jtoc(vm, (uint32_t)a[0], b, sizeof b); if (MM(vm)->h.log) MM(vm)->h.log(b); return 0; }
static int n_vibrate(VM *vm, int32_t *a) { if (MM(vm)->h.vibrate) MM(vm)->h.vibrate(a[0]); return 0; }
static int n_quit(VM *vm, int32_t *a) { (void)a; MM(vm)->quit = 1; return 0; }
static void rms_path(Midp *m, VM *vm, uint32_t name, char *out, int cap) {
    char n[80]; jtoc(vm, name, n, sizeof n); int o = 0; const char *p = "/user/jm_";
    while (*p && o < cap - 1) out[o++] = *p++;
    for (const char *q = m->id; *q && o < cap - 1; q++) out[o++] = *q;
    if (o < cap - 1) out[o++] = '_';
    for (int i = 0; n[i] && o < cap - 8; i++) { char c = n[i]; out[o++] = (char)(((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? c : '_'); }
    out[o++] = '.'; out[o++] = 'r'; out[o++] = 'm'; out[o++] = 's'; out[o] = 0;
}
static int n_rms_read(VM *vm, int32_t *a) {
    Midp *m = MM(vm); char path[128]; uint32_t len; rms_path(m, vm, (uint32_t)a[0], path, sizeof path);
    uint8_t *b = m->h.read_file ? m->h.read_file(path, &len) : 0; if (!b) { vm->ret[0] = 0; return 0; }
    uint32_t r = new_arr(vm, 8, (int32_t)len); if (!r) { free(b); return vm_throw(vm, "java/lang/OutOfMemoryError"); }
    memcpy(vm->heap + r + 8, b, len); free(b); vm->ret[0] = (int32_t)r; return 0;
}
static int n_rms_write(VM *vm, int32_t *a) {
    Midp *m = MM(vm); char path[128]; rms_path(m, vm, (uint32_t)a[0], path, sizeof path);
    if (!a[1]) return vm_throw(vm, "java/lang/NullPointerException");
    vm->ret[0] = m->h.write_file ? m->h.write_file(path, vm->heap + (uint32_t)a[1] + 8, (uint32_t)a[2]) == 0 : 0; return 0;
}
static int n_rms_delete(VM *vm, int32_t *a) { Midp *m = MM(vm); char path[128]; rms_path(m, vm, (uint32_t)a[0], path, sizeof path); vm->ret[0] = m->h.delete_file ? m->h.delete_file(path) == 0 : 0; return 0; }
static int n_rms_size(VM *vm, int32_t *a) {
    Midp *m = MM(vm); char path[128]; uint32_t len = 0; rms_path(m, vm, (uint32_t)a[0], path, sizeof path);
    uint8_t *b = m->h.read_file ? m->h.read_file(path, &len) : 0; if (b) free(b); vm->ret[0] = (int32_t)len; return 0;
}

/* ---------- graphics ---------- */
typedef struct { uint16_t *px; uint8_t *al; int w, h; int ox, oy, x0, y0, x1, y1; uint16_t col; uint32_t rgb; } Gc;
static uint16_t c565(uint32_t rgb) { return (uint16_t)(((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x7e0) | ((rgb >> 3) & 0x1f)); }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static int gc_get(VM *vm, Midp *m, uint32_t g, Gc *c) {
    int32_t *f = FLD(vm, g, 0); uint32_t img = (uint32_t)f[m->gs.img];
    if (!img) { c->px = m->scr; c->al = 0; c->w = MIDP_W; c->h = MIDP_H_; }
    else {
        int32_t *fi = FLD(vm, img, 0); uint32_t pa = (uint32_t)fi[m->is.px], aa = (uint32_t)fi[m->is.alpha];
        if (!pa) return 0;
        c->px = (uint16_t *)(vm->heap + pa + 8); c->al = aa ? vm->heap + aa + 8 : 0; c->w = fi[m->is.w]; c->h = fi[m->is.h];
    }
    c->ox = f[m->gs.tx]; c->oy = f[m->gs.ty];
    c->x0 = imax(0, c->ox + f[m->gs.cx]); c->y0 = imax(0, c->oy + f[m->gs.cy]);
    c->x1 = imin(c->w, c->ox + f[m->gs.cx] + f[m->gs.cw]); c->y1 = imin(c->h, c->oy + f[m->gs.cy] + f[m->gs.ch]);
    c->rgb = (uint32_t)f[m->gs.color]; c->col = c565(c->rgb); return 1;
}
static void hspan(Gc *c, int y, int xa, int xb, uint16_t col) {          /* [xa, xb) device coordinates */
    if (y < c->y0 || y >= c->y1) return; if (xa < c->x0) xa = c->x0; if (xb > c->x1) xb = c->x1;
    uint16_t *p = c->px + y * c->w; for (int x = xa; x < xb; x++) p[x] = col;
}
static void plot(Gc *c, int x, int y, uint16_t col) { if (x >= c->x0 && x < c->x1 && y >= c->y0 && y < c->y1) c->px[y * c->w + x] = col; }
static int n_fillrect(VM *vm, int32_t *a) {
    Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0;
    int x = a[1] + c.ox, y = a[2] + c.oy, w = a[3], h = a[4]; if (w <= 0 || h <= 0) return 0;
    for (int j = y; j < y + h; j++) hspan(&c, j, x, x + w, c.col);
    return 0;
}
static int n_drawline(VM *vm, int32_t *a) {
    Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0;
    int x0 = a[1] + c.ox, y0 = a[2] + c.oy, x1 = a[3] + c.ox, y1 = a[4] + c.oy;
    if (y0 == y1) { hspan(&c, y0, imin(x0, x1), imax(x0, x1) + 1, c.col); return 0; }
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int n = 0; n < 4096; n++) {
        plot(&c, x0, y0, c.col); if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return 0;
}
static int n_filltri(VM *vm, int32_t *a) {
    Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0;
    int x[3] = { a[1] + c.ox, a[3] + c.ox, a[5] + c.ox }, y[3] = { a[2] + c.oy, a[4] + c.oy, a[6] + c.oy };
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2 - i; j++) if (y[j] > y[j + 1]) { int t = y[j]; y[j] = y[j + 1]; y[j + 1] = t; t = x[j]; x[j] = x[j + 1]; x[j + 1] = t; }
    for (int yy = imax(y[0], c.y0); yy <= imin(y[2], c.y1 - 1); yy++) {
        int xa, xb;
        if (y[2] == y[0]) { xa = imin(x[0], imin(x[1], x[2])); xb = imax(x[0], imax(x[1], x[2])); }
        else {
            xa = x[0] + (int)((int64_t)(x[2] - x[0]) * (yy - y[0]) / (y[2] - y[0]));
            if (yy < y[1]) xb = y[1] == y[0] ? x[1] : x[0] + (int)((int64_t)(x[1] - x[0]) * (yy - y[0]) / (y[1] - y[0]));
            else xb = y[2] == y[1] ? x[1] : x[1] + (int)((int64_t)(x[2] - x[1]) * (yy - y[1]) / (y[2] - y[1]));
        }
        if (xa > xb) { int t = xa; xa = xb; xb = t; }
        hspan(&c, yy, xa, xb + 1, c.col);
    }
    return 0;
}
static int angle_in(int deg, int sa, int aa) {                   /* is the angle (0..359, counter-clockwise from 3 o'clock) inside the arc */
    if (aa >= 360 || aa <= -360) return 1;
    int d = deg - sa; if (aa >= 0) { d %= 360; if (d < 0) d += 360; return d <= aa; }
    d = sa - deg; d %= 360; if (d < 0) d += 360; return d <= -aa;
}
static void ellipse(Gc *c, int x, int y, int w, int h, int sa, int aa, int fill) {
    if (w <= 0 || h <= 0) return;
    double rx = w / 2.0, ry = h / 2.0, cx = x + rx, cy = y + ry; int full = aa >= 360 || aa <= -360;
    for (int yy = imax(y, c->y0); yy <= imin(y + h, c->y1 - 1); yy++) {
        double dy = (yy + 0.5 - cy) / ry; if (dy > 1 || dy < -1) continue;
        double hw = rx * jm_sqrt(1 - dy * dy);
        int xa = (int)jm_floor(cx - hw + 0.5), xb = (int)jm_floor(cx + hw + 0.5);
        if (full && fill) { hspan(c, yy, xa, xb, c->col); continue; }
        for (int xx = imax(xa, c->x0); xx < imin(xb, c->x1); xx++) {
            int edge = xx == xa || xx == xb - 1 || yy == y || yy == y + h - 1 || (hw - jm_floor(hw) < 2 && 0);
            if (!fill && !edge) { double dx2 = (xx + 0.5 - cx) / rx; double v = dx2 * dx2 + dy * dy; if (v < 0.80) continue; }
            if (!full) { double ang = jm_atan2(-(yy + 0.5 - cy) / ry, (xx + 0.5 - cx) / rx) * 180.0 / 3.14159265358979; int deg = (int)(ang < 0 ? ang + 360.5 : ang + 0.5); if (!angle_in(deg % 360, sa, aa)) continue; }
            plot(c, xx, yy, c->col);
        }
    }
}
static int n_fillarc(VM *vm, int32_t *a) { Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0; ellipse(&c, a[1] + c.ox, a[2] + c.oy, a[3], a[4], a[5], a[6], 1); return 0; }
static int n_drawarc(VM *vm, int32_t *a) { Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0; ellipse(&c, a[1] + c.ox, a[2] + c.oy, a[3] + 1, a[4] + 1, a[5], a[6], 0); return 0; }
static int n_fillround(VM *vm, int32_t *a) {
    Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0;
    int x = a[1] + c.ox, y = a[2] + c.oy, w = a[3], h = a[4], rx = a[5] / 2, ry = a[6] / 2; if (w <= 0 || h <= 0) return 0;
    if (rx > w / 2) rx = w / 2; if (ry > h / 2) ry = h / 2;
    for (int j = 0; j < h; j++) {
        int inset = 0;
        if (ry > 0 && rx > 0 && (j < ry || j >= h - ry)) { double dy = (j < ry ? ry - j - 0.5 : j - (h - ry) + 0.5) / ry; if (dy > 1) dy = 1; inset = rx - (int)(rx * jm_sqrt(1 - dy * dy) + 0.5); }
        hspan(&c, y + j, x + inset, x + w - inset, c.col);
    }
    return 0;
}
static const uint8_t *glyph(uint32_t cp) {
    if (cp >= 32 && cp <= 126) return kbd_font[cp - 32];
    for (int i = 0; i < (int)(sizeof kbd_font_extra_cp / 2); i++) if (kbd_font_extra_cp[i] == cp) return kbd_font[KBD_FONT_N_ASCII + i];
    return kbd_font['?' - 32];
}
static int n_drawstring(VM *vm, int32_t *a) {
    Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0;
    if (!a[1]) return vm_throw(vm, "java/lang/NullPointerException");
    int n; const uint16_t *s = jchars(vm, (uint32_t)a[1], &n); int anchor = a[4];
    uint32_t font = (uint32_t)*FLD(vm, (uint32_t)a[0], m->gs.font); int large = font && *FLD(vm, font, m->fs_size) == 16;
    int cw = large ? 9 : 8, hh = large ? 14 : 12, base = large ? 11 : 10;
    int x = a[2] + c.ox, y = a[3] + c.oy, w = n * cw;
    if (anchor & 1) x -= w / 2; else if (anchor & 8) x -= w;
    if (anchor & 32) y -= hh; else if (anchor & 64) y -= base; else if (anchor & 2) y -= hh / 2;
    if (y >= c.y1 || y + hh <= c.y0 || x >= c.x1 || x + w <= c.x0) return 0;
    for (int i = 0; i < n; i++, x += cw) {
        if (x + cw <= c.x0 || x >= c.x1) continue;
        const uint8_t *g = glyph(s[i]);
        for (int j = 0; j < 12; j++) { uint8_t row = g[j]; if (!row) continue;
            for (int k = 0; k < 8; k++) if (row & (0x80 >> k)) { plot(&c, x + k, y + j, c.col); if (large) plot(&c, x + k + 1, y + j, c.col); } }
    }
    return 0;
}
static int n_drawimage0(VM *vm, int32_t *a) {
    Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0;
    uint32_t img = (uint32_t)a[1]; if (!img) return vm_throw(vm, "java/lang/NullPointerException");
    int32_t *fi = FLD(vm, img, 0); uint32_t pa = (uint32_t)fi[m->is.px], aa = (uint32_t)fi[m->is.alpha]; if (!pa) return 0;
    const uint16_t *sp = (const uint16_t *)(vm->heap + pa + 8); const uint8_t *sal = aa ? vm->heap + aa + 8 : 0; int iw = fi[m->is.w], ih = fi[m->is.h];
    int dx = a[2] + c.ox, dy = a[3] + c.oy, sx = a[4], sy = a[5], w = a[6], h = a[7], tr = a[8];
    if (sx < 0 || sy < 0 || w <= 0 || h <= 0 || sx + w > iw || sy + h > ih) return vm_throw(vm, "java/lang/IllegalArgumentException");
    int ow = (tr & 4) ? h : w, oh = (tr & 4) ? w : h;
    if (sp == c.px) return 0;
    for (int j = 0; j < oh; j++) {
        int Y = dy + j; if (Y < c.y0 || Y >= c.y1) continue;
        for (int i = 0; i < ow; i++) {
            int X = dx + i; if (X < c.x0 || X >= c.x1) continue; int u, v;
            switch (tr) { case 1: u = i; v = h - 1 - j; break; case 2: u = w - 1 - i; v = j; break; case 3: u = w - 1 - i; v = h - 1 - j; break;
                case 4: u = j; v = i; break; case 5: u = j; v = h - 1 - i; break; case 6: u = w - 1 - j; v = i; break; case 7: u = w - 1 - j; v = h - 1 - i; break;
                default: u = i; v = j; }
            int k = (sy + v) * iw + sx + u; if (sal && sal[k] < 128) continue; c.px[Y * c.w + X] = sp[k];
        }
    }
    return 0;
}
static int n_drawrgb(VM *vm, int32_t *a) {
    Midp *m = MM(vm); Gc c; if (!gc_get(vm, m, (uint32_t)a[0], &c)) return 0;
    if (!a[1]) return vm_throw(vm, "java/lang/NullPointerException");
    const int32_t *rgb = (const int32_t *)(vm->heap + (uint32_t)a[1] + 8); uint32_t len = HW(vm, (uint32_t)a[1])[1];
    int off = a[2], scan = a[3], x = a[4] + c.ox, y = a[5] + c.oy, w = a[6], h = a[7], alpha = a[8];
    for (int j = 0; j < h; j++) { int Y = y + j; if (Y < c.y0 || Y >= c.y1) continue;
        for (int i = 0; i < w; i++) { int X = x + i; if (X < c.x0 || X >= c.x1) continue; int64_t k = (int64_t)off + (int64_t)j * scan + i; if (k < 0 || k >= (int64_t)len) continue;
            uint32_t p = (uint32_t)rgb[k]; uint32_t al = alpha ? p >> 24 : 255; if (al == 0) continue; uint16_t *d = &c.px[Y * c.w + X];
            if (al >= 255) *d = c565(p);
            else { uint32_t o = *d; int dr = (o >> 11) << 3, dg = ((o >> 5) & 63) << 2, db = (o & 31) << 3; int sr = (p >> 16) & 255, sg = (p >> 8) & 255, sb = p & 255;
                *d = c565((uint32_t)((((sr * al + dr * (255 - al)) / 255) << 16) | (((sg * al + dg * (255 - al)) / 255) << 8) | ((sb * al + db * (255 - al)) / 255))); } } }
    return 0;
}
static int n_decode(VM *vm, int32_t *a) {
    Midp *m = MM(vm); uint32_t dst = (uint32_t)a[0], d = (uint32_t)a[1]; int off = a[2], len = a[3]; vm->ret[0] = 0;
    if (!d) return vm_throw(vm, "java/lang/NullPointerException");
    if (off < 0 || len < 0 || (uint32_t)off + (uint32_t)len > HW(vm, d)[1]) return vm_throw(vm, "java/lang/ArrayIndexOutOfBoundsException");
    uint16_t *px = 0; uint8_t *al = 0; int w, h;
    if (png_decode(vm->heap + d + 8 + off, (uint32_t)len, &px, &al, &w, &h)) return 0;
    uint32_t pa = new_arr(vm, 9, w * h);
    if (!pa) { free(px); free(al); return vm_throw(vm, "java/lang/OutOfMemoryError"); }
    int32_t *f = FLD(vm, dst, 0); f[m->is.w] = w; f[m->is.h] = h; f[m->is.px] = (int32_t)pa;
    memcpy(vm->heap + pa + 8, px, (size_t)w * h * 2); free(px);
    if (al) { uint32_t aa = new_arr(vm, 8, w * h); if (!aa) { free(al); return vm_throw(vm, "java/lang/OutOfMemoryError"); } f = FLD(vm, dst, 0); f[m->is.alpha] = (int32_t)aa; memcpy(vm->heap + aa + 8, al, (size_t)w * h); free(al); }
    vm->ret[0] = 1; return 0;
}

/* ---------- native table ---------- */
#define S1 1
static const NativeEntry nat[] = {
    {"java/lang/Object", "getClass", "()Ljava/lang/Class;", n_getclass, 0}, {"java/lang/Object", "toString", "()Ljava/lang/String;", n_obj_tostring, 0},
    {"java/lang/Object", "wait", "()V", n_wait0, 0}, {"java/lang/Object", "wait", "(J)V", n_wait1, 0},
    {"java/lang/Object", "notify", "()V", n_notify, 0}, {"java/lang/Object", "notifyAll", "()V", n_notifyall, 0},
    {"java/lang/String", "length", "()I", n_str_length, 0}, {"java/lang/String", "charAt", "(I)C", n_str_charat, 0},
    {"java/lang/String", "equals", "(Ljava/lang/Object;)Z", n_str_equals, 0}, {"java/lang/String", "hashCode", "()I", n_str_hash, 0},
    {"java/lang/String", "compareTo", "(Ljava/lang/String;)I", n_str_compare, 0},
    {"java/lang/String", "indexOfC", "(II)I", n_str_indexc, 0}, {"java/lang/String", "indexOfS", "(Ljava/lang/String;I)I", n_str_indexs, 0},
    {"java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", n_arraycopy, S1}, {"java/lang/System", "currentTimeMillis", "()J", n_millis, S1},
    {"java/lang/System", "identityHashCode", "(Ljava/lang/Object;)I", n_idhash, S1}, {"java/lang/System", "gc", "()V", n_gc, S1},
    {"java/lang/Runtime", "freeMemory", "()J", n_freemem, 0}, {"java/lang/Runtime", "totalMemory", "()J", n_totalmem, 0},
    {"java/lang/Thread", "spawn0", "()Z", n_spawn0, 0}, {"java/lang/Thread", "isAlive", "()Z", n_isalive, 0}, {"java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", n_curthread, S1},
    {"java/lang/Thread", "sleep", "(J)V", n_sleep, S1}, {"java/lang/Thread", "yield", "()V", n_yield, S1},
    {"java/lang/Class", "getName", "()Ljava/lang/String;", n_class_name, 0}, {"java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;", n_forname, S1},
    {"java/lang/Class", "newInstance", "()Ljava/lang/Object;", n_newinstance, 0},
    {"java/lang/Float", "floatToIntBits", "(F)I", n_f2i, S1}, {"java/lang/Float", "intBitsToFloat", "(I)F", n_f2i, S1},
    {"java/lang/Double", "doubleToLongBits", "(D)J", n_d2l, S1}, {"java/lang/Double", "longBitsToDouble", "(J)D", n_d2l, S1},
    {"java/lang/Math", "sin", "(D)D", n_sin, S1}, {"java/lang/Math", "cos", "(D)D", n_cos, S1},
    {"java/lang/Math", "sqrt", "(D)D", n_sqrt, S1}, {"java/lang/Math", "floor", "(D)D", n_floor, S1}, {"java/lang/Math", "ceil", "(D)D", n_ceil, S1},
    {"java/lang/Math", "atan", "(D)D", n_atan, S1},
    {"jvmhost/Host", "res", "(Ljava/lang/String;)[B", n_res, S1}, {"jvmhost/Host", "pollEvent", "()I", n_pollevent, S1}, {"jvmhost/Host", "pollArg", "()I", n_pollarg, S1},
    {"jvmhost/Host", "width", "()I", n_width, S1}, {"jvmhost/Host", "height", "()I", n_height, S1}, {"jvmhost/Host", "present", "(Ljava/lang/Object;)V", n_present, S1},
    {"jvmhost/Host", "trace", "()Ljava/lang/String;", n_trace, S1},
    {"jvmhost/Host", "log", "(Ljava/lang/String;)V", n_log, S1}, {"jvmhost/Host", "vibrate", "(I)V", n_vibrate, S1}, {"jvmhost/Host", "quit", "()V", n_quit, S1},
    {"jvmhost/Host", "rmsRead", "(Ljava/lang/String;)[B", n_rms_read, S1}, {"jvmhost/Host", "rmsWrite", "(Ljava/lang/String;[BI)Z", n_rms_write, S1},
    {"jvmhost/Host", "rmsDelete", "(Ljava/lang/String;)Z", n_rms_delete, S1}, {"jvmhost/Host", "rmsSize", "(Ljava/lang/String;)I", n_rms_size, S1},
    {"javax/microedition/lcdui/Graphics", "fillRect", "(IIII)V", n_fillrect, 0}, {"javax/microedition/lcdui/Graphics", "drawLine", "(IIII)V", n_drawline, 0},
    {"javax/microedition/lcdui/Graphics", "fillTriangle", "(IIIIII)V", n_filltri, 0}, {"javax/microedition/lcdui/Graphics", "fillArc", "(IIIIII)V", n_fillarc, 0},
    {"javax/microedition/lcdui/Graphics", "drawArc", "(IIIIII)V", n_drawarc, 0}, {"javax/microedition/lcdui/Graphics", "fillRoundRect", "(IIIIII)V", n_fillround, 0},
    {"javax/microedition/lcdui/Graphics", "drawString", "(Ljava/lang/String;III)V", n_drawstring, 0},
    {"javax/microedition/lcdui/Graphics", "drawImage0", "(Ljavax/microedition/lcdui/Image;IIIIIII)V", n_drawimage0, 0},
    {"javax/microedition/lcdui/Graphics", "drawRGB", "([IIIIIIIZ)V", n_drawrgb, 0},
    {"javax/microedition/lcdui/Image", "decode0", "(Ljavax/microedition/lcdui/Image;[BII)Z", n_decode, S1},
};

/* ---------- loader / lifecycle ---------- */
static const uint8_t *loader(const char *name, uint32_t *len, void *ud) {
    Midp *m = (Midp *)ud; char path[200]; size_t n = strlen(name);
    if (n > 180) return 0;
    memcpy(path, name, n); memcpy(path + n, ".class", 7);
    uint8_t *b = jar_read(&m->jar, path, len); if (b) return b;
    return jar_read(&m->rt, path, len);
}
static void on_die(VM *vm, JThread *t, uint32_t exc) {
    Midp *m = MM(vm); char buf[200]; int o = 0; (void)t;
    const char *p = "Uncaught: "; while (*p) buf[o++] = *p++;
    if (exc) { const char *cn = vm_class_name(vm_obj_class(vm, exc)); while (*cn && o < 120) { buf[o++] = *cn == '/' ? '.' : *cn; cn++; }
        uint32_t msg = (uint32_t)*FLD(vm, exc, 0);
        if (msg && vm_obj_class(vm, msg) == vm->c_string) { buf[o++] = ':'; buf[o++] = ' '; int n; const uint16_t *c = jchars(vm, msg, &n); for (int i = 0; i < n && o < 190; i++) buf[o++] = (char)(c[i] < 128 ? c[i] : '?'); } }
    else { const char *e = vm->err ? vm->err : "vm error"; while (*e && o < 120) buf[o++] = *e++;
        if (vm->err_m) { const char *cn = vm_method_class(vm->err_m); buf[o++] = ' '; buf[o++] = '['; while (*cn && o < 190) { buf[o++] = *cn == '/' ? '.' : *cn; cn++; } buf[o++] = ']'; } }
    buf[o] = 0; if (m->h.log) m->h.log(buf);
    for (int i = 0; i < vm->ntrace && m->h.log; i++) { char l[160]; int q = 0; const char *cn = vm_method_class(vm->tr_m[i]), *mn = vm_method_name(vm->tr_m[i]);
        l[q++] = ' '; l[q++] = ' '; l[q++] = 'a'; l[q++] = 't'; l[q++] = ' '; while (*cn && q < 110) { l[q++] = *cn == '/' ? '.' : *cn; cn++; } l[q++] = '.'; while (*mn && q < 140) l[q++] = *mn++;
        l[q++] = ' '; l[q++] = '@'; { int pc = vm->tr_pc[i], dgt[8], nd = 0; if (!pc) dgt[nd++] = 0; while (pc) { dgt[nd++] = pc % 10; pc /= 10; } while (nd) l[q++] = (char)('0' + dgt[--nd]); } l[q] = 0; m->h.log(l); }
    memcpy(m->errbuf, buf, (size_t)(o < 159 ? o : 159) + 1); m->errbuf[159] = 0; m->err = m->errbuf;
}
Midp *midp_create(const MidpHost *h, uint8_t *jar, uint32_t jarlen, uint8_t *rt, uint32_t rtlen, const char *id, uint32_t heap_bytes) {
    Midp *m = (Midp *)calloc(1, sizeof(Midp)); if (!m) return 0;
    m->h = *h; m->jarbuf = jar; m->rtbuf = rt;
    for (int i = 0; i < 23 && id[i]; i++) m->id[i] = id[i];
    if (jar_open(&m->jar, jar, jarlen) || jar_open(&m->rt, rt, rtlen)) { free(m); return 0; }
    m->scr = (uint16_t *)calloc(MIDP_W * MIDP_H_, 2); if (!m->scr) { free(m); return 0; }
    for (int i = 0; i < MIDP_W * MIDP_H_; i++) m->scr[i] = 0xffff;
    if (vm_init(&m->vm, heap_bytes, 16384, h->ticks)) { free(m->scr); free(m); return 0; }
    m->vm.user = m; m->vm.load_class = loader; m->vm.load_ud = m; m->vm.on_die = on_die; vm_set_natives(&m->vm, nat, (int)(sizeof nat / sizeof nat[0]));
    vm_pin(&m->vm, &m->ctab); m->t0 = h->ticks();
    return m;
}
static char *midlet_class(Midp *m, char *out, int cap) {                                      /* MIDlet-1: name, icon, class */
    uint32_t n; uint8_t *mf = jar_read(&m->jar, "META-INF/MANIFEST.MF", &n); if (!mf) return 0;
    char *txt = (char *)malloc(n + 1), *w = txt;
    for (uint32_t i = 0; i < n; i++) { if (mf[i] == '\r') continue; if (mf[i] == '\n' && i + 1 < n && mf[i + 1] == ' ') { i++; continue; } *w++ = (char)mf[i]; }
    *w = 0; free(mf);
    char *p = txt; char *res = 0;
    while (*p) { if (!strncmp(p, "MIDlet-1:", 9)) { p += 9; char *e = p; while (*e && *e != '\n') e++;
            int commas = 0; char *q = p; while (q < e && commas < 2) { if (*q == ',') commas++; q++; }
            if (commas == 2) { while (q < e && *q == ' ') q++; int o = 0; while (q < e && *q != ' ' && o < cap - 1) { out[o++] = *q == '.' ? '/' : *q; q++; } out[o] = 0; res = out; }
            break; }
        while (*p && *p != '\n') p++; if (*p) p++; }
    free(txt); return res;
}
int midp_boot(Midp *m) {
    VM *vm = &m->vm; Class *g = vm_find_class(vm, "javax/microedition/lcdui/Graphics"), *im = vm_find_class(vm, "javax/microedition/lcdui/Image"), *fo = vm_find_class(vm, "javax/microedition/lcdui/Font");
    if (!g || !im || !fo || !vm_find_class(vm, "java/lang/String")) { m->err = "runtime classes missing (rt.pak)"; return -1; }
    m->gs.img = vm_field_slot(g, "img"); m->gs.color = vm_field_slot(g, "color"); m->gs.tx = vm_field_slot(g, "tx"); m->gs.ty = vm_field_slot(g, "ty");
    m->gs.cx = vm_field_slot(g, "cx"); m->gs.cy = vm_field_slot(g, "cy"); m->gs.cw = vm_field_slot(g, "cw"); m->gs.ch = vm_field_slot(g, "ch"); m->gs.font = vm_field_slot(g, "font");
    m->is.w = vm_field_slot(im, "w"); m->is.h = vm_field_slot(im, "h"); m->is.px = vm_field_slot(im, "px"); m->is.alpha = vm_field_slot(im, "alpha");
    m->fs_size = vm_field_slot(fo, "size");
    if (!midlet_class(m, m->midlet, sizeof m->midlet)) { m->err = "no MIDlet-1 in manifest"; return -2; }
    Class *bc = vm_find_class(vm, "jvmhost/Boot"); if (!bc) { m->err = "jvmhost/Boot missing"; return -3; }
    uint32_t b = vm_new_object(vm, bc); uint32_t s = new_str(vm, m->midlet); if (!b || !s) { m->err = "out of memory"; return -4; }
    *FLD(vm, b, vm_field_slot(bc, "cls")) = (int32_t)s;
    if (vm_spawn_sized(vm, b, 16384) < 0) { m->err = "cannot start thread"; return -5; }
    return 0;
}
void midp_run(Midp *m, uint32_t budget_ms) {
    if (m->finished) return;
    int alive = vm_run(&m->vm, budget_ms);
    if (m->quit || alive == 0) m->finished = 1;
}
void midp_event(Midp *m, int type, int arg) { int n = (m->evw + 1) % EVQ; if (n == m->evr) return; m->ev[m->evw][0] = type; m->ev[m->evw][1] = arg; m->evw = n; }
const uint16_t *midp_screen(Midp *m, int *dirty) { if (dirty) { *dirty = m->dirty; } m->dirty = 0; return m->scr; }
int midp_finished(Midp *m) { return m->finished; }
const char *midp_error(Midp *m) { return m->err; }
uint32_t midp_heap_used(Midp *m) { return m->vm.used; }
void midp_destroy(Midp *m) { if (!m) return; free(m->scr); free(m->jarbuf); free(m->rtbuf); free(m); }
