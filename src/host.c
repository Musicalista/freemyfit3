/* PC test host: loads a MIDP .jar, runs the MIDlet headless, dumps frames as PPM. */
#include "vm.h"
#include "jar.h"
#include "m3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SW 240
#define SH 320
static uint32_t fb[SW * SH]; static float zbuf[SW * SH];
static uint32_t cur_color;
static long long vclock = 1000000;
static int frames, max_frames = 30, dump_every = 0;
static long render_calls, draw_calls;
static Jar jar;
static const char *rt_dir = "rt_out", *out_prefix = "frame";
static uint32_t pending[8]; static int npending;
static uint32_t *class_objs; static int n_class_objs;

/* ---------- loader: jar first, then precompiled runtime dir ---------- */
static const uint8_t *loader(const char *name, uint32_t *len, void *ud) {
    char path[600]; (void)ud;
    snprintf(path, sizeof path, "%s.class", name);
    uint8_t *b = jar_read(&jar, path, len); if (b) return b;
    snprintf(path, sizeof path, "%s/%s.class", rt_dir, name);
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) { fclose(f); return 0; } fclose(f); *len = (uint32_t)n; return b;
}
/* ---------- helpers ---------- */
static uint32_t *OBJ(VM *vm, int32_t r) { return (uint32_t *)(vm->heap + (uint32_t)r); }
static char *jstr(VM *vm, int32_t ref) {
    if (!ref) return strdup("");
    uint32_t arr = (uint32_t)((int32_t *)(vm->heap + ref + 8))[0]; uint32_t n = arr ? OBJ(vm, arr)[1] : 0;
    char *s = malloc(n + 1); for (uint32_t i = 0; i < n; i++) s[i] = (char)((uint16_t *)(vm->heap + arr + 8))[i]; s[n] = 0; return s;
}
static int esize(uint32_t t) { t &= 0xff; switch (t) { case 4: case 8: return 1; case 5: case 9: return 2; case 11: case 7: return 8; default: return 4; } }
static int64_t arg64(int32_t *a) { return (int64_t)(((uint64_t)(uint32_t)a[1] << 32) | (uint32_t)a[0]); }
static void ret64(VM *vm, int64_t v) { vm->ret[0] = (int32_t)v; vm->ret[1] = (int32_t)(v >> 32); }
static double argd(int32_t *a) { int64_t v = arg64(a); double d; memcpy(&d, &v, 8); return d; }
static void retd(VM *vm, double d) { int64_t v; memcpy(&v, &d, 8); ret64(vm, v); }

/* ---------- natives ---------- */
static int n_res(VM *vm, int32_t *a) {
    char *n = jstr(vm, a[0]); const char *p = n[0] == '/' ? n + 1 : n; uint32_t len; uint8_t *b = jar_read(&jar, p, &len); free(n);
    if (!b) { vm->ret[0] = 0; return 0; }
    uint32_t r = vm_new_array(vm, 8, (int32_t)len); if (!r) { free(b); return vm_throw(vm, "java/lang/OutOfMemoryError"); }
    memcpy(vm->heap + r + 8, b, len); free(b); vm->ret[0] = (int32_t)r; return 0;
}
static int n_sleep(VM *vm, int32_t *a) { (void)vm; vclock += arg64(a); return 0; }
static int n_spawn(VM *vm, int32_t *a) { (void)vm; if (npending < 8) pending[npending++] = (uint32_t)a[0]; return 0; }
static int n_poll(VM *vm, int32_t *a) { (void)a; vm->ret[0] = 0; return 0; }
static int n_width(VM *vm, int32_t *a) { (void)a; vm->ret[0] = SW; return 0; }
static int n_height(VM *vm, int32_t *a) { (void)a; vm->ret[0] = SH; return 0; }
static void dump_ppm(const char *name) {
    FILE *f = fopen(name, "wb"); if (!f) return; fprintf(f, "P6\n%d %d\n255\n", SW, SH);
    for (int i = 0; i < SW * SH; i++) { uint8_t c[3] = { (uint8_t)(fb[i] >> 16), (uint8_t)(fb[i] >> 8), (uint8_t)fb[i] }; fwrite(c, 1, 3, f); }
    fclose(f);
}
static int n_present(VM *vm, int32_t *a) {
    (void)a; frames++;
    if (dump_every && frames % dump_every == 0) { char nm[256]; snprintf(nm, sizeof nm, "%s_%04d.ppm", out_prefix, frames); dump_ppm(nm); }
    if (frames >= max_frames) {
        char nm[256]; snprintf(nm, sizeof nm, "%s_last.ppm", out_prefix); dump_ppm(nm);
        printf("[host] %d frames, %ld render calls, %ld draw calls, heap used %u / %u, vclock +%lld ms\n", frames, render_calls, draw_calls, vm->heap_top, vm->heap_size, vclock - 1000000);
        exit(0);
    }
    return 0;
}
static int n_setcolor(VM *vm, int32_t *a) { (void)vm; cur_color = (uint32_t)a[0] & 0xffffff; return 0; }
static int n_fillrect(VM *vm, int32_t *a) {
    (void)vm; draw_calls++;
    int x0 = a[0], y0 = a[1], x1 = a[0] + a[2], y1 = a[1] + a[3];
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > SW) x1 = SW; if (y1 > SH) y1 = SH;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) fb[y * SW + x] = cur_color;
    return 0;
}
static int n_drawline(VM *vm, int32_t *a) {
    (void)vm; draw_calls++; int x0 = a[0], y0 = a[1], x1 = a[2], y1 = a[3];
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) { if ((unsigned)x0 < SW && (unsigned)y0 < SH) fb[y0 * SW + x0] = cur_color;
        if (x0 == x1 && y0 == y1) break; int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
    return 0;
}
static int n_drawstring(VM *vm, int32_t *a) { /* placeholder: 6x8 block per char */
    draw_calls++; char *s = jstr(vm, a[0]); int x = a[1], y = a[2];
    for (const char *p = s; *p; p++, x += 6) if (*p != ' ') for (int j = 0; j < 7; j++) for (int i = 0; i < 5; i++)
        if ((unsigned)(x + i) < SW && (unsigned)(y + j) < SH) fb[(y + j) * SW + x + i] = cur_color;
    free(s); return 0;
}
static int n_arraycopy(VM *vm, int32_t *a) {
    uint32_t s = (uint32_t)a[0], d = (uint32_t)a[2]; int sp = a[1], dp = a[3], n = a[4];
    if (!s || !d) return vm_throw(vm, "java/lang/NullPointerException");
    uint32_t *hs = OBJ(vm, s), *hd = OBJ(vm, d);
    if (!(hs[0] & 0x80000000u) || !(hd[0] & 0x80000000u) || hs[0] != hd[0]) return vm_throw(vm, "java/lang/ClassCastException");
    if (sp < 0 || dp < 0 || n < 0 || (uint32_t)sp + n > hs[1] || (uint32_t)dp + n > hd[1]) return vm_throw(vm, "java/lang/ArrayIndexOutOfBoundsException");
    int es = esize(hs[0]); memmove(vm->heap + d + 8 + dp * es, vm->heap + s + 8 + sp * es, (size_t)n * es); return 0;
}
static int n_millis(VM *vm, int32_t *a) { (void)a; ret64(vm, vclock); return 0; }
static int n_sin(VM *vm, int32_t *a) { retd(vm, sin(argd(a))); return 0; }
static int n_cos(VM *vm, int32_t *a) { retd(vm, cos(argd(a))); return 0; }
static int n_tan(VM *vm, int32_t *a) { retd(vm, tan(argd(a))); return 0; }
static int n_sqrt(VM *vm, int32_t *a) { retd(vm, sqrt(argd(a))); return 0; }
static int n_floor(VM *vm, int32_t *a) { retd(vm, floor(argd(a))); return 0; }
static int n_ceil(VM *vm, int32_t *a) { retd(vm, ceil(argd(a))); return 0; }
static int n_getclass(VM *vm, int32_t *a) {
    uint32_t id = OBJ(vm, a[0])[0]; Class *oc = vm_obj_class(vm, (uint32_t)a[0]); if (id & 0x80000000u) id = (uint32_t)vm_class_id(oc);
    if ((int)id >= n_class_objs) { int nn = (int)id + 64; class_objs = realloc(class_objs, nn * sizeof(uint32_t)); memset(class_objs + n_class_objs, 0, (nn - n_class_objs) * sizeof(uint32_t)); n_class_objs = nn; }
    if (!class_objs[id]) { Class *cc = vm_find_class(vm, "java/lang/Class"); uint32_t o = vm_new_object(vm, cc); ((int32_t *)(vm->heap + o + 8))[0] = (int32_t)id; class_objs[id] = o; }
    vm->ret[0] = (int32_t)class_objs[id]; return 0;
}
static int n_printstack(VM *vm, int32_t *a) { printf("[exception] %s\n", vm_class_name(vm_obj_class(vm, (uint32_t)a[0]))); return 0; }
static int32_t *IARR(VM *vm, int32_t r, int *len) { if (!r) { *len = 0; return 0; } *len = (int)OBJ(vm, r)[1]; return (int32_t *)(vm->heap + (uint32_t)r + 8); }
static int last_eye_frame = -1; static M3dTex tex_cache; static uint32_t tex_ref;
static int n_bind(VM *vm, int32_t *a) { (void)vm; (void)a; m3d_target(fb, zbuf, SW, SH); m3d_clear_depth(); return 0; }
static int n_render(VM *vm, int32_t *a) { /* this,tex,x,y,fl,ef,cmd,num,coords,normals,tex,colors */
    render_calls++;
    int lc, lu, lk; int32_t *co = IARR(vm, a[8], &lc), *uv = IARR(vm, a[10], &lu), *cl = IARR(vm, a[11], &lk);
    int num = a[7]; if (!co || !uv || num <= 0 || lc < num * 12 || lu < num * 8) return 0;
    int32_t texref = a[1] ? a[1] : (a[5] ? ((int32_t *)(vm->heap + (uint32_t)a[5] + 8))[3] : 0); if (!texref) return 0;
    uint32_t dref = (uint32_t)((int32_t *)(vm->heap + (uint32_t)texref + 8))[0];
    if (dref != tex_ref) { if (m3d_parse_bmp(vm->heap + dref + 8, OBJ(vm, (int32_t)dref)[1], &tex_cache)) return 0; tex_ref = dref; }
    int32_t *fl = (int32_t *)(vm->heap + (uint32_t)a[4] + 8); int32_t *at = (int32_t *)(vm->heap + (uint32_t)fl[0] + 8);
    M3dView v; static const int mi[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    for (int i = 0; i < 12; i++) v.m[i] = (i % 4 == 3) ? (float)at[mi[i]] : (float)at[mi[i]] / 4096.0f;
    v.cx = (float)fl[1]; v.cy = (float)fl[2]; v.znear = (float)fl[3]; v.zfar = (float)fl[4];
    v.f = (SW * 0.5f) / tanf((float)fl[5] * 3.14159265f / 4096.0f);
    if (getenv("EYE") && frames != last_eye_frame) { last_eye_frame = frames;
        float tx = v.m[3], ty = v.m[7], tz = v.m[11];
        printf("vm frame %3d eye=(%.3f %.3f %.3f)\n", frames + 1, -(v.m[0]*tx + v.m[4]*ty + v.m[8]*tz) / 256.0f, -(v.m[1]*tx + v.m[5]*ty + v.m[9]*tz) / 256.0f, -(v.m[2]*tx + v.m[6]*ty + v.m[10]*tz) / 256.0f); }
    m3d_quads(&v, &tex_cache, num, co, uv, lk >= num ? cl : 0); return 0;
}
static const NativeEntry nat[] = {
    {"jvmhost/Host", "res", "(Ljava/lang/String;)[B", n_res, 1}, {"jvmhost/Host", "sleep", "(J)V", n_sleep, 1},
    {"jvmhost/Host", "spawn", "(Ljava/lang/Object;)V", n_spawn, 1}, {"jvmhost/Host", "pollKey", "()I", n_poll, 1},
    {"jvmhost/Host", "width", "()I", n_width, 1}, {"jvmhost/Host", "height", "()I", n_height, 1},
    {"jvmhost/Host", "present", "()V", n_present, 1}, {"jvmhost/Host", "setColor", "(I)V", n_setcolor, 1},
    {"jvmhost/Host", "fillRect", "(IIII)V", n_fillrect, 1}, {"jvmhost/Host", "drawLine", "(IIII)V", n_drawline, 1},
    {"jvmhost/Host", "drawString", "(Ljava/lang/String;III)V", n_drawstring, 1},
    {"java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", n_arraycopy, 1},
    {"java/lang/System", "currentTimeMillis", "()J", n_millis, 1},
    {"java/lang/Math", "sin", "(D)D", n_sin, 1}, {"java/lang/Math", "cos", "(D)D", n_cos, 1}, {"java/lang/Math", "tan", "(D)D", n_tan, 1},
    {"java/lang/Math", "sqrt", "(D)D", n_sqrt, 1}, {"java/lang/Math", "floor", "(D)D", n_floor, 1}, {"java/lang/Math", "ceil", "(D)D", n_ceil, 1},
    {"java/lang/Object", "getClass", "()Ljava/lang/Class;", n_getclass, 0},
    {"java/lang/Throwable", "printStackTrace", "()V", n_printstack, 0},
    {"com/mascotcapsule/micro3d/v3/Graphics3D", "bind", "(Ljavax/microedition/lcdui/Graphics;)V", n_bind, 0},
    {"com/mascotcapsule/micro3d/v3/Graphics3D", "renderPrimitives",
     "(Lcom/mascotcapsule/micro3d/v3/Texture;IILcom/mascotcapsule/micro3d/v3/FigureLayout;Lcom/mascotcapsule/micro3d/v3/Effect3D;II[I[I[I[I)V", n_render, 0},
};

static char *midlet_class(void) {
    uint32_t n; uint8_t *m = jar_read(&jar, "META-INF/MANIFEST.MF", &n); if (!m) return 0;
    char *txt = malloc(n + 1), *w = txt; for (uint32_t i = 0; i < n; i++) { if (m[i] == '\r') continue;
        if (m[i] == '\n' && i + 1 < n && m[i + 1] == ' ') { i++; continue; } *w++ = (char)m[i]; } *w = 0;
    char *p = strstr(txt, "MIDlet-1:"); if (!p) return 0; p += 9; char *e = strchr(p, '\n'); if (e) *e = 0;
    char *c1 = strchr(p, ','); char *c2 = c1 ? strchr(c1 + 1, ',') : 0; if (!c2) return 0; c2++; while (*c2 == ' ') c2++;
    char *r = strdup(c2); for (char *q = r; *q; q++) if (*q == '.') *q = '/'; while (strlen(r) && r[strlen(r) - 1] == ' ') r[strlen(r) - 1] = 0; return r;
}
int main(int argc, char **argv) {
    if (argc < 2) { puts("usage: host game.jar [frames] [dump_every]"); return 1; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { puts("cannot open jar"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); uint8_t *img = malloc(n); if (fread(img, 1, n, f) != (size_t)n) return 1; fclose(f);
    if (jar_open(&jar, img, (uint32_t)n)) { puts("bad jar"); return 1; }
    if (argc > 2) max_frames = atoi(argv[2]); if (argc > 3) dump_every = atoi(argv[3]);
    VM vm; if (vm_init(&vm, 64u << 20, 65536)) return 2;
    vm_set_natives(&vm, nat, (int)(sizeof nat / sizeof nat[0])); vm.load_class = loader;
    char *mc = midlet_class(); if (!mc) { puts("no MIDlet-1 in manifest"); return 3; }
    printf("[host] MIDlet class: %s\n", mc);
    Class *c = vm_find_class(&vm, mc); if (!c) { puts("midlet class not found"); return 3; }
    uint32_t obj = vm_new_object(&vm, c); int32_t arg = (int32_t)obj; int r;
    Method *init = vm_find_method(c, "<init>", "()V"); if (!init) { puts("no <init>"); return 3; }
    if ((r = vm_invoke_static(&vm, init, &arg, 1))) { printf("init failed: %s\n", vm.exc ? vm_exc_name(&vm) : "(vm error)"); return 4; }
    Method *st = vm_find_method(c, "_start", "()V"); if (!st) { puts("no _start"); return 3; }
    if ((r = vm_invoke_static(&vm, st, &arg, 1))) { printf("startApp failed: %s\n", vm.exc ? vm_exc_name(&vm) : "(vm error)"); return 4; }
    printf("[host] startApp ok, %d pending thread(s)\n", npending);
    for (int i = 0; i < npending; i++) {
        int32_t t = (int32_t)pending[i]; Method *run = vm_find_method(vm_obj_class(&vm, pending[i]), "run", "()V");
        if (!run) { puts("no run"); continue; }
        r = vm_invoke_static(&vm, run, &t, 1);
        if (r) { printf("thread died: %s (frames=%d)\n", vm.exc ? vm_exc_name(&vm) : "(vm error)", frames); return 5; }
    }
    printf("[host] threads finished, frames=%d\n", frames); return 0;
}
