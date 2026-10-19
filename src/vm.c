#include "vm.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define ACC_STATIC 0x0008
#define ACC_NATIVE 0x0100
#define ARR_FLAG 0x80000000u
#define MAX_DEPTH 200

typedef struct { uint16_t start, end, handler, catch_type; } Exc;
typedef struct { uint8_t tag; uint16_t a, b; uint32_t u; int64_t l; char *s; void *res; Class *rcls; int32_t ri; } CP;
typedef struct { char *name, *desc; uint16_t acc; int slot; uint16_t cv; } Field;
struct Method {
    char *name, *desc; uint16_t acc, max_stack, max_locals; const uint8_t *code; uint32_t code_len;
    Exc *exc; uint16_t nexc; NativeFn native; int argsz, retsz; Class *cls;
};
struct Class {
    char *name; Class *super; Class **ifaces; int niface; CP *cp; int ncp; Field *fields; int nfields;
    Method *methods; int nmethods; int nslots; int32_t *statics; int nstatics; int state, id; uint16_t acc;
};

/* ---------- helpers ---------- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int16_t s16(const uint8_t *p) { return (int16_t)rd16(p); }
static char *dupn(const uint8_t *s, int n) { char *r = malloc(n + 1); memcpy(r, s, n); r[n] = 0; return r; }
static char *dups(const char *s) { return dupn((const uint8_t *)s, (int)strlen(s)); }

static void desc_sizes(const char *d, int *args, int *ret) {
    int n = 0; const char *p = d + 1;
    while (*p != ')') {
        if (*p == 'J' || *p == 'D') { n += 2; p++; }
        else if (*p == 'L') { n++; while (*p != ';') p++; p++; }
        else if (*p == '[') { n++; while (*p == '[') p++; if (*p == 'L') { while (*p != ';') p++; } p++; }
        else { n++; p++; }
    }
    *args = n; p++;
    *ret = (*p == 'V') ? 0 : (*p == 'J' || *p == 'D') ? 2 : 1;
}

/* ---------- builtin native classes ---------- */
typedef struct { const char *name, *super; int nfields; } BClass;
static const BClass bclasses[] = {
    {"java/lang/Object", 0, 0}, {"java/lang/String", "java/lang/Object", 1},
    {"java/lang/Throwable", "java/lang/Object", 1}, {"java/lang/Exception", "java/lang/Throwable", 0},
    {"java/lang/Error", "java/lang/Throwable", 0}, {"java/lang/RuntimeException", "java/lang/Exception", 0},
    {"java/lang/ArithmeticException", "java/lang/RuntimeException", 0},
    {"java/lang/NullPointerException", "java/lang/RuntimeException", 0},
    {"java/lang/ClassCastException", "java/lang/RuntimeException", 0},
    {"java/lang/IllegalArgumentException", "java/lang/RuntimeException", 0},
    {"java/lang/IndexOutOfBoundsException", "java/lang/RuntimeException", 0},
    {"java/lang/ArrayIndexOutOfBoundsException", "java/lang/IndexOutOfBoundsException", 0},
    {"java/lang/StringIndexOutOfBoundsException", "java/lang/IndexOutOfBoundsException", 0},
    {"java/lang/NegativeArraySizeException", "java/lang/RuntimeException", 0},
    {"java/lang/InterruptedException", "java/lang/Exception", 0},
    {"java/lang/OutOfMemoryError", "java/lang/Error", 0},
};
static int n_nop(VM *vm, int32_t *a) { (void)vm; (void)a; return 0; }
static int n_thr_init_s(VM *vm, int32_t *a) { ((int32_t *)(vm->heap + a[0] + 8))[0] = a[1]; return 0; }
static int n_str_length(VM *vm, int32_t *a) {
    uint32_t v = (uint32_t)((int32_t *)(vm->heap + a[0] + 8))[0];
    vm->ret[0] = v ? (int32_t)((uint32_t *)(vm->heap + v))[1] : 0; return 0; }
static int n_str_charat(VM *vm, int32_t *a) {
    uint32_t v = (uint32_t)((int32_t *)(vm->heap + a[0] + 8))[0];
    uint32_t len = v ? ((uint32_t *)(vm->heap + v))[1] : 0;
    if ((uint32_t)a[1] >= len) return -2;
    vm->ret[0] = ((uint16_t *)(vm->heap + v + 8))[a[1]]; return 0; }
static int n_obj_equals(VM *vm, int32_t *a) { (void)vm; vm->ret[0] = a[0] == a[1]; return 0; }
static int n_obj_hash(VM *vm, int32_t *a) { vm->ret[0] = a[0]; return 0; }
static int n_str_equals(VM *vm, int32_t *a) {
    uint32_t o = (uint32_t)a[1]; vm->ret[0] = 0;
    if (a[0] == a[1]) { vm->ret[0] = 1; return 0; }
    if (!o || ((uint32_t *)(vm->heap + o))[0] != (uint32_t)vm_class_id(vm_find_class(vm, "java/lang/String"))) return 0;
    uint32_t x = (uint32_t)((int32_t *)(vm->heap + a[0] + 8))[0], y = (uint32_t)((int32_t *)(vm->heap + o + 8))[0];
    uint32_t lx = x ? ((uint32_t *)(vm->heap + x))[1] : 0, ly = y ? ((uint32_t *)(vm->heap + y))[1] : 0;
    if (lx != ly) return 0;
    vm->ret[0] = !lx || !memcmp(vm->heap + x + 8, vm->heap + y + 8, lx * 2); return 0; }
static const NativeEntry builtin_natives[] = {
    {"java/lang/Object", "equals", "(Ljava/lang/Object;)Z", n_obj_equals, 0},
    {"java/lang/Object", "hashCode", "()I", n_obj_hash, 0},
    {"java/lang/String", "equals", "(Ljava/lang/Object;)Z", n_str_equals, 0},
    {"java/lang/Object", "<init>", "()V", n_nop, 0},
    {"java/lang/Throwable", "<init>", "()V", n_nop, 0},
    {"java/lang/Throwable", "<init>", "(Ljava/lang/String;)V", n_thr_init_s, 0},
    {"java/lang/String", "length", "()I", n_str_length, 0},
    {"java/lang/String", "charAt", "(I)C", n_str_charat, 0},
};
#define NB (int)(sizeof(builtin_natives) / sizeof(builtin_natives[0]))

static NativeFn find_native(VM *vm, const char *cls, const char *name, const char *desc) {
    int i;
    for (i = 0; i < NB; i++) { const NativeEntry *e = &builtin_natives[i];
        if (!strcmp(e->cls, cls) && !strcmp(e->name, name) && !strcmp(e->desc, desc)) return e->fn; }
    for (i = 0; i < vm->nnatives; i++) { const NativeEntry *e = &vm->natives[i];
        if (!strcmp(e->cls, cls) && !strcmp(e->name, name) && !strcmp(e->desc, desc)) return e->fn; }
    return 0;
}

/* ---------- heap ---------- */
static uint32_t heap_alloc(VM *vm, uint32_t bytes, int reserve_ok) {
    uint32_t sz = (bytes + 7) & ~7u, lim = reserve_ok ? vm->heap_size : vm->heap_size - 2048;
    if (vm->heap_top + sz > lim) return 0;
    uint32_t r = vm->heap_top; vm->heap_top += sz; memset(vm->heap + r, 0, sz); return r;
}
static int elem_size(int t) { switch (t) { case 4: case 8: return 1; case 5: case 9: return 2; case 11: case 7: return 8; default: return 4; } }
uint32_t vm_new_array(VM *vm, int type, int32_t len) {
    if (len < 0) return 0;
    uint32_t r = heap_alloc(vm, 8 + (uint32_t)len * elem_size(type), 0); if (!r) return 0;
    ((uint32_t *)(vm->heap + r))[0] = ARR_FLAG | (uint32_t)type; ((uint32_t *)(vm->heap + r))[1] = (uint32_t)len; return r;
}
uint32_t vm_new_object(VM *vm, Class *c) {
    uint32_t r = heap_alloc(vm, 8 + c->nslots * 4, 1); if (!r) return 0;
    ((uint32_t *)(vm->heap + r))[0] = (uint32_t)c->id; return r;
}
uint32_t vm_new_string(VM *vm, const char *s) {
    int n = (int)strlen(s); uint32_t arr = vm_new_array(vm, 5, n); if (!arr) return 0;
    for (int i = 0; i < n; i++) ((uint16_t *)(vm->heap + arr + 8))[i] = (uint8_t)s[i];
    Class *sc = vm_find_class(vm, "java/lang/String"); uint32_t o = vm_new_object(vm, sc);
    if (o) ((int32_t *)(vm->heap + o + 8))[0] = (int32_t)arr; return o;
}

/* ---------- class loading ---------- */
static Class *add_class(VM *vm, Class *c) {
    if (vm->nclasses == vm->cap_classes) { vm->cap_classes = vm->cap_classes ? vm->cap_classes * 2 : 32;
        vm->classes = realloc(vm->classes, vm->cap_classes * sizeof(Class *)); }
    c->id = vm->nclasses; vm->classes[vm->nclasses++] = c; return c;
}
static Class *make_builtin(VM *vm, const BClass *b) {
    Class *c = calloc(1, sizeof(Class)); c->name = dups(b->name);
    c->super = b->super ? vm_find_class(vm, b->super) : 0;
    c->nslots = (c->super ? c->super->nslots : 0) + b->nfields; c->state = 2;
    int cnt = 0, i;
    for (i = 0; i < NB; i++) if (!strcmp(builtin_natives[i].cls, b->name)) cnt++;
    for (i = 0; i < vm->nnatives; i++) if (!strcmp(vm->natives[i].cls, b->name)) cnt++;
    c->methods = calloc(cnt ? cnt : 1, sizeof(Method));
    for (i = 0; i < NB + vm->nnatives; i++) {
        const NativeEntry *e = i < NB ? &builtin_natives[i] : &vm->natives[i - NB];
        if (strcmp(e->cls, b->name)) continue;
        Method *m = &c->methods[c->nmethods++]; m->name = (char *)e->name; m->desc = (char *)e->desc; m->native = e->fn;
        m->acc = (uint16_t)(e->is_static ? (ACC_STATIC | ACC_NATIVE) : ACC_NATIVE); m->cls = c;
        desc_sizes(m->desc, &m->argsz, &m->retsz); if (!e->is_static) m->argsz++;
    }
    return add_class(vm, c);
}
static Class *parse_class(VM *vm, const uint8_t *b) {
    if (rd32(b) != 0xCAFEBABE) return 0;
    int ncp = rd16(b + 8), i; const uint8_t *p = b + 10;
    Class *c = calloc(1, sizeof(Class)); c->ncp = ncp; c->cp = calloc(ncp, sizeof(CP));
    for (i = 1; i < ncp; i++) {
        CP *e = &c->cp[i]; e->tag = *p++;
        switch (e->tag) {
        case 1: { int n = rd16(p); e->s = dupn(p + 2, n); p += 2 + n; break; }
        case 3: case 4: e->u = rd32(p); p += 4; break;
        case 5: case 6: e->l = (int64_t)(((uint64_t)rd32(p) << 32) | rd32(p + 4)); p += 8; i++; break;
        case 7: case 8: e->a = rd16(p); p += 2; break;
        case 9: case 10: case 11: case 12: e->a = rd16(p); e->b = rd16(p + 2); p += 4; break;
        case 15: p += 3; break; case 16: p += 2; break; case 18: p += 4; break;
        default: return 0; }
    }
    c->acc = rd16(p); c->name = dups(c->cp[c->cp[rd16(p + 2)].a].s);
    uint16_t sup = rd16(p + 4); c->niface = rd16(p + 6); p += 8;
    c->super = sup ? vm_find_class(vm, c->cp[c->cp[sup].a].s) : 0;
    c->ifaces = calloc(c->niface ? c->niface : 1, sizeof(Class *));
    for (i = 0; i < c->niface; i++, p += 2) c->ifaces[i] = vm_find_class(vm, c->cp[c->cp[rd16(p)].a].s);
    c->nslots = c->super ? c->super->nslots : 0;
    c->nfields = rd16(p); p += 2; c->fields = calloc(c->nfields ? c->nfields : 1, sizeof(Field));
    for (i = 0; i < c->nfields; i++) {
        Field *f = &c->fields[i]; f->acc = rd16(p); f->name = c->cp[rd16(p + 2)].s; f->desc = c->cp[rd16(p + 4)].s;
        int w = (f->desc[0] == 'J' || f->desc[0] == 'D') ? 2 : 1;
        if (f->acc & ACC_STATIC) { f->slot = c->nstatics; c->nstatics += w; } else { f->slot = c->nslots; c->nslots += w; }
        int na = rd16(p + 6); p += 8;
        while (na--) { if (!strcmp(c->cp[rd16(p)].s, "ConstantValue")) f->cv = rd16(p + 6); p += 6 + rd32(p + 2); }
    }
    c->statics = calloc(c->nstatics ? c->nstatics : 1, 4);
    c->nmethods = rd16(p); p += 2; c->methods = calloc(c->nmethods ? c->nmethods : 1, sizeof(Method));
    for (i = 0; i < c->nmethods; i++) {
        Method *m = &c->methods[i]; m->acc = rd16(p); m->name = c->cp[rd16(p + 2)].s; m->desc = c->cp[rd16(p + 4)].s; m->cls = c;
        desc_sizes(m->desc, &m->argsz, &m->retsz); if (!(m->acc & ACC_STATIC)) m->argsz++;
        int na = rd16(p + 6); p += 8;
        while (na--) {
            const char *an = c->cp[rd16(p)].s; uint32_t al = rd32(p + 2); const uint8_t *q = p + 6;
            if (!strcmp(an, "Code")) {
                m->max_stack = rd16(q); m->max_locals = rd16(q + 2); m->code_len = rd32(q + 4); m->code = q + 8;
                const uint8_t *e = q + 8 + m->code_len; m->nexc = rd16(e); e += 2;
                m->exc = calloc(m->nexc ? m->nexc : 1, sizeof(Exc));
                for (int k = 0; k < m->nexc; k++, e += 8) { m->exc[k].start = rd16(e); m->exc[k].end = rd16(e + 2); m->exc[k].handler = rd16(e + 4); m->exc[k].catch_type = rd16(e + 6); }
            }
            p += 6 + al;
        }
    }
    add_class(vm, c);
    for (i = 0; i < c->nmethods; i++) if (c->methods[i].acc & ACC_NATIVE)
        c->methods[i].native = find_native(vm, c->name, c->methods[i].name, c->methods[i].desc);
    return c;
}
Class *vm_find_class(VM *vm, const char *name) {
    int i;
    for (i = 0; i < vm->nclasses; i++) if (!strcmp(vm->classes[i]->name, name)) return vm->classes[i];
    for (i = 0; i < (int)(sizeof(bclasses) / sizeof(bclasses[0])); i++) if (!strcmp(bclasses[i].name, name)) return make_builtin(vm, &bclasses[i]);
    if (vm->load_class) { uint32_t n; const uint8_t *d = vm->load_class(name, &n, vm->load_ud); if (d) return parse_class(vm, d); }
    return 0;
}
const char *vm_class_name(Class *c) { return c->name; }
Method *vm_find_method(Class *c, const char *name, const char *desc) {
    for (; c; c = c->super) {
        for (int i = 0; i < c->nmethods; i++) if (!strcmp(c->methods[i].name, name) && !strcmp(c->methods[i].desc, desc)) return &c->methods[i];
        for (int i = 0; i < c->niface; i++) { Method *m = vm_find_method(c->ifaces[i], name, desc); if (m) return m; }
    }
    return 0;
}
static Field *find_field(Class *c, const char *name, Class **owner) {
    for (; c; c = c->super) {
        for (int i = 0; i < c->nfields; i++) if (!strcmp(c->fields[i].name, name)) { *owner = c; return &c->fields[i]; }
        for (int i = 0; i < c->niface; i++) { Field *f = find_field(c->ifaces[i], name, owner); if (f) return f; }
    }
    return 0;
}
static int is_sub(Class *c, Class *t) {
    for (; c; c = c->super) { if (c == t) return 1; for (int i = 0; i < c->niface; i++) if (is_sub(c->ifaces[i], t)) return 1; }
    return 0;
}
static Class *obj_class(VM *vm, uint32_t r) { uint32_t id = ((uint32_t *)(vm->heap + r))[0]; return (id & ARR_FLAG) ? vm_find_class(vm, "java/lang/Object") : vm->classes[id]; }


/* ---------- float helpers ---------- */
static float bf(int32_t v) { float f; memcpy(&f, &v, 4); return f; }
static int32_t fb(float f) { int32_t v; memcpy(&v, &f, 4); return v; }
static double bd(int64_t v) { double d; memcpy(&d, &v, 8); return d; }
static int64_t db(double d) { int64_t v; memcpy(&v, &d, 8); return v; }
static int32_t d2i(double d) { return d != d ? 0 : d >= 2147483647.0 ? 2147483647 : d <= -2147483648.0 ? (-2147483647 - 1) : (int32_t)d; }
static int64_t d2l(double d) { return d != d ? 0 : d >= 9.2233720368547758e18 ? 0x7fffffffffffffffLL : d <= -9.2233720368547758e18 ? (-0x7fffffffffffffffLL - 1) : (int64_t)d; }

/* ---------- interpreter ---------- */
static int exec(VM *vm, Method *m, int32_t *locals);
static int call_method(VM *vm, Method *m, int32_t *args) {
    if (vm->depth > MAX_DEPTH) return -1;
    if (m->native) return m->native(vm, args);
    if (!m->code) return -1;
    vm->depth++; int r = exec(vm, m, args); vm->depth--; return r;
}
/* caller must have set vm->top to the first free stack slot */
static int init_class(VM *vm, Class *c) {
    if (c->state) return 0; c->state = 1;
    if (c->super && init_class(vm, c->super)) return 1;
    for (int i = 0; i < c->nfields; i++) { Field *f = &c->fields[i]; if (!f->cv || !(f->acc & ACC_STATIC)) continue;
        CP *e = &c->cp[f->cv]; int32_t *s = c->statics + f->slot;
        if (e->tag == 8) s[0] = (int32_t)vm_new_string(vm, c->cp[e->a].s);
        else if (e->tag == 5 || e->tag == 6) { s[0] = (int32_t)e->l; s[1] = (int32_t)(e->l >> 32); } else s[0] = (int32_t)e->u; }
    Method *cl = 0; for (int i = 0; i < c->nmethods; i++) if (!strcmp(c->methods[i].name, "<clinit>")) cl = &c->methods[i];
    if (cl) { int32_t *base = vm->top; vm->top = base + cl->max_locals + cl->max_stack + 2; int r = call_method(vm, cl, base); vm->top = base; if (r) return 1; }
    return 0;
}
static int throw_new(VM *vm, const char *cn) { Class *c = vm_find_class(vm, cn); vm->exc = c ? vm_new_object(vm, c) : 0; return 1; }
int vm_throw(VM *vm, const char *cn) { return throw_new(vm, cn); }
Class *vm_obj_class(VM *vm, uint32_t r) { return obj_class(vm, r); }
int vm_class_id(Class *c) { return c->id; }
const char *vm_exc_name(VM *vm) { return vm->exc ? obj_class(vm, vm->exc)->name : "(none)"; }

int vm_init(VM *vm, uint32_t heap_bytes, uint32_t stack_slots) {
    memset(vm, 0, sizeof *vm); vm->heap = malloc(heap_bytes); vm->stack = malloc(stack_slots * 4);
    if (!vm->heap || !vm->stack) return -1;
    vm->heap_size = heap_bytes; vm->heap_top = 16; vm->stack_size = stack_slots; vm->top = vm->stack; return 0;
}
void vm_set_natives(VM *vm, const NativeEntry *t, int n) { vm->natives = t; vm->nnatives = n; }
int vm_invoke_static(VM *vm, Method *m, const int32_t *args, int nargs) {
    int32_t *base = vm->top; vm->exc = 0;
    for (int i = 0; i < nargs; i++) base[i] = args[i];
    vm->top = base + m->max_locals + m->max_stack + 2;
    if (init_class(vm, m->cls)) { vm->top = base; return 1; }
    int r = call_method(vm, m, base); vm->top = base; return r ? 1 : 0;
}

#define PUSH(x) (*sp++ = (int32_t)(x))
#define POP() (*--sp)
#define PUSHL(x) do { int64_t _v = (x); sp[0] = (int32_t)_v; sp[1] = (int32_t)(_v >> 32); sp += 2; } while (0)
static int64_t popl(int32_t **spp) { int32_t *sp = *spp - 2; *spp = sp; return (int64_t)((uint64_t)(uint32_t)sp[0] | ((uint64_t)(uint32_t)sp[1] << 32)); }
#define POPL() popl(&sp)
#define AT(r) ((uint8_t *)(vm->heap + (r)))
#define THROW(name) do { throw_new(vm, name); goto exception; } while (0)
#define NPE(r) do { if (!(r)) THROW("java/lang/NullPointerException"); } while (0)
#define ACHK(r, i) do { NPE(r); if ((uint32_t)(i) >= ((uint32_t *)AT(r))[1]) THROW("java/lang/ArrayIndexOutOfBoundsException"); } while (0)
#define INIT(c) do { vm->top = sp; if (init_class(vm, (c))) goto exception; } while (0)

static int exec(VM *vm, Method *m, int32_t *locals) {
    Class *cls = m->cls; const uint8_t *code = m->code; uint32_t pc = 0, ipc = 0;
    int32_t *sbase = locals + m->max_locals, *sp = sbase; int32_t tmp;
    if (sbase + m->max_stack + 2 > vm->stack + vm->stack_size) return -1;
    for (;;) {
        ipc = pc; uint8_t op = code[pc++];
        switch (op) {
        case 0x00: break;
        case 0x01: PUSH(0); break;
        case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: PUSH((int)op - 3); break;
        case 0x09: case 0x0a: PUSHL(op - 9); break;
        case 0x0b: PUSH(0); break; case 0x0c: PUSH(0x3f800000); break; case 0x0d: PUSH(0x40000000); break;
        case 0x0e: PUSHL(0); break; case 0x0f: PUSHL(0x3ff0000000000000LL); break;
        case 0x10: PUSH((int8_t)code[pc]); pc++; break;
        case 0x11: PUSH(s16(code + pc)); pc += 2; break;
        case 0x12: case 0x13: {
            int idx; if (op == 0x12) idx = code[pc++]; else { idx = rd16(code + pc); pc += 2; }
            CP *e = &cls->cp[idx];
            if (e->tag == 8) { if (!e->ri) { e->ri = (int32_t)vm_new_string(vm, cls->cp[e->a].s); if (!e->ri) THROW("java/lang/OutOfMemoryError"); } PUSH(e->ri); }
            else PUSH(e->u);
            break; }
        case 0x14: { CP *e = &cls->cp[rd16(code + pc)]; pc += 2; PUSHL(e->l); break; }
        case 0x15: case 0x17: case 0x19: PUSH(locals[code[pc++]]); break;
        case 0x16: case 0x18: { int i = code[pc++]; PUSH(locals[i]); PUSH(locals[i + 1]); break; }
        case 0x1a: case 0x1b: case 0x1c: case 0x1d: PUSH(locals[op - 0x1a]); break;
        case 0x1e: case 0x1f: case 0x20: case 0x21: { int i = op - 0x1e; PUSH(locals[i]); PUSH(locals[i + 1]); break; }
        case 0x22: case 0x23: case 0x24: case 0x25: PUSH(locals[op - 0x22]); break;
        case 0x26: case 0x27: case 0x28: case 0x29: { int i = op - 0x26; PUSH(locals[i]); PUSH(locals[i + 1]); break; }
        case 0x2a: case 0x2b: case 0x2c: case 0x2d: PUSH(locals[op - 0x2a]); break;
        case 0x2e: case 0x30: case 0x32: { int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); PUSH(((int32_t *)(AT(r) + 8))[i]); break; }
        case 0x2f: case 0x31: { int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); PUSHL(((int64_t *)(AT(r) + 8))[i]); break; }
        case 0x33: { int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); PUSH(((int8_t *)(AT(r) + 8))[i]); break; }
        case 0x34: { int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); PUSH(((uint16_t *)(AT(r) + 8))[i]); break; }
        case 0x35: { int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); PUSH(((int16_t *)(AT(r) + 8))[i]); break; }
        case 0x36: case 0x38: case 0x3a: locals[code[pc++]] = POP(); break;
        case 0x37: case 0x39: { int i = code[pc++]; locals[i + 1] = POP(); locals[i] = POP(); break; }
        case 0x3b: case 0x3c: case 0x3d: case 0x3e: locals[op - 0x3b] = POP(); break;
        case 0x3f: case 0x40: case 0x41: case 0x42: { int i = op - 0x3f; locals[i + 1] = POP(); locals[i] = POP(); break; }
        case 0x43: case 0x44: case 0x45: case 0x46: locals[op - 0x43] = POP(); break;
        case 0x47: case 0x48: case 0x49: case 0x4a: { int i = op - 0x47; locals[i + 1] = POP(); locals[i] = POP(); break; }
        case 0x4b: case 0x4c: case 0x4d: case 0x4e: locals[op - 0x4b] = POP(); break;
        case 0x4f: case 0x51: case 0x53: { int32_t v = POP(); int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); ((int32_t *)(AT(r) + 8))[i] = v; break; }
        case 0x50: case 0x52: { int64_t v = POPL(); int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); ((int64_t *)(AT(r) + 8))[i] = v; break; }
        case 0x54: { int32_t v = POP(); int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); ((int8_t *)(AT(r) + 8))[i] = (int8_t)v; break; }
        case 0x55: case 0x56: { int32_t v = POP(); int i = POP(); uint32_t r = (uint32_t)POP(); ACHK(r, i); ((uint16_t *)(AT(r) + 8))[i] = (uint16_t)v; break; }
        case 0x57: sp--; break; case 0x58: sp -= 2; break;
        case 0x59: sp[0] = sp[-1]; sp++; break;
        case 0x5a: sp[0] = sp[-1]; sp[-1] = sp[-2]; sp[-2] = sp[0]; sp++; break;
        case 0x5b: sp[0] = sp[-1]; sp[-1] = sp[-2]; sp[-2] = sp[-3]; sp[-3] = sp[0]; sp++; break;
        case 0x5c: sp[0] = sp[-2]; sp[1] = sp[-1]; sp += 2; break;
        case 0x5d: sp[1] = sp[-1]; sp[0] = sp[-2]; sp[-1] = sp[-3]; sp[-2] = sp[1]; sp[-3] = sp[0]; sp += 2; break;
        case 0x5e: sp[1] = sp[-1]; sp[0] = sp[-2]; sp[-1] = sp[-3]; sp[-2] = sp[-4]; sp[-3] = sp[1]; sp[-4] = sp[0]; sp += 2; break;
        case 0x5f: tmp = sp[-1]; sp[-1] = sp[-2]; sp[-2] = tmp; break;
        case 0x60: { uint32_t b = (uint32_t)POP(); sp[-1] = (int32_t)((uint32_t)sp[-1] + b); break; }
        case 0x64: { uint32_t b = (uint32_t)POP(); sp[-1] = (int32_t)((uint32_t)sp[-1] - b); break; }
        case 0x68: { uint32_t b = (uint32_t)POP(); sp[-1] = (int32_t)((uint32_t)sp[-1] * b); break; }
        case 0x6c: case 0x70: { int32_t b = POP(), a = POP(); if (!b) THROW("java/lang/ArithmeticException");
            if (b == -1) PUSH(op == 0x6c ? (int32_t)(0u - (uint32_t)a) : 0); else PUSH(op == 0x6c ? a / b : a % b); break; }
        case 0x74: sp[-1] = (int32_t)(0u - (uint32_t)sp[-1]); break;
        case 0x78: { int b = POP(); sp[-1] = (int32_t)((uint32_t)sp[-1] << (b & 31)); break; }
        case 0x7a: { int b = POP(); sp[-1] = sp[-1] >> (b & 31); break; }
        case 0x7c: { int b = POP(); sp[-1] = (int32_t)((uint32_t)sp[-1] >> (b & 31)); break; }
        case 0x7e: { int b = POP(); sp[-1] &= b; break; }
        case 0x80: { int b = POP(); sp[-1] |= b; break; }
        case 0x82: { int b = POP(); sp[-1] ^= b; break; }
        case 0x61: { uint64_t b = (uint64_t)POPL(), a = (uint64_t)POPL(); PUSHL((int64_t)(a + b)); break; }
        case 0x65: { uint64_t b = (uint64_t)POPL(), a = (uint64_t)POPL(); PUSHL((int64_t)(a - b)); break; }
        case 0x69: { uint64_t b = (uint64_t)POPL(), a = (uint64_t)POPL(); PUSHL((int64_t)(a * b)); break; }
        case 0x6d: case 0x71: { int64_t b = POPL(), a = POPL(); if (!b) THROW("java/lang/ArithmeticException");
            if (b == -1) PUSHL(op == 0x6d ? (int64_t)(0ull - (uint64_t)a) : 0); else PUSHL(op == 0x6d ? a / b : a % b); break; }
        case 0x75: { uint64_t a = (uint64_t)POPL(); PUSHL((int64_t)(0ull - a)); break; }
        case 0x79: { int b = POP(); uint64_t a = (uint64_t)POPL(); PUSHL((int64_t)(a << (b & 63))); break; }
        case 0x7b: { int b = POP(); int64_t a = POPL(); PUSHL(a >> (b & 63)); break; }
        case 0x7d: { int b = POP(); uint64_t a = (uint64_t)POPL(); PUSHL((int64_t)(a >> (b & 63))); break; }
        case 0x7f: { int64_t b = POPL(), a = POPL(); PUSHL(a & b); break; }
        case 0x81: { int64_t b = POPL(), a = POPL(); PUSHL(a | b); break; }
        case 0x83: { int64_t b = POPL(), a = POPL(); PUSHL(a ^ b); break; }
        case 0x62: { float b = bf(POP()); sp[-1] = fb(bf(sp[-1]) + b); break; }
        case 0x66: { float b = bf(POP()); sp[-1] = fb(bf(sp[-1]) - b); break; }
        case 0x6a: { float b = bf(POP()); sp[-1] = fb(bf(sp[-1]) * b); break; }
        case 0x6e: { float b = bf(POP()); sp[-1] = fb(bf(sp[-1]) / b); break; }
        case 0x72: { float b = bf(POP()); sp[-1] = fb(fmodf(bf(sp[-1]), b)); break; }
        case 0x76: sp[-1] = fb(-bf(sp[-1])); break;
        case 0x63: { double b = bd(POPL()), a = bd(POPL()); PUSHL(db(a + b)); break; }
        case 0x67: { double b = bd(POPL()), a = bd(POPL()); PUSHL(db(a - b)); break; }
        case 0x6b: { double b = bd(POPL()), a = bd(POPL()); PUSHL(db(a * b)); break; }
        case 0x6f: { double b = bd(POPL()), a = bd(POPL()); PUSHL(db(a / b)); break; }
        case 0x73: { double b = bd(POPL()), a = bd(POPL()); PUSHL(db(fmod(a, b))); break; }
        case 0x77: { double a = bd(POPL()); PUSHL(db(-a)); break; }
        case 0x86: { int32_t a = POP(); PUSH(fb((float)a)); break; }
        case 0x87: { int32_t a = POP(); PUSHL(db((double)a)); break; }
        case 0x89: { int64_t a = POPL(); PUSH(fb((float)a)); break; }
        case 0x8a: { int64_t a = POPL(); PUSHL(db((double)a)); break; }
        case 0x8b: { float a = bf(POP()); PUSH(d2i(a)); break; }
        case 0x8c: { float a = bf(POP()); PUSHL(d2l(a)); break; }
        case 0x8d: { float a = bf(POP()); PUSHL(db((double)a)); break; }
        case 0x8e: { double a = bd(POPL()); PUSH(d2i(a)); break; }
        case 0x8f: { double a = bd(POPL()); PUSHL(d2l(a)); break; }
        case 0x90: { double a = bd(POPL()); PUSH(fb((float)a)); break; }
        case 0x95: case 0x96: { float b = bf(POP()), a = bf(POP()); PUSH(a > b ? 1 : a == b ? 0 : a < b ? -1 : (op == 0x96 ? 1 : -1)); break; }
        case 0x97: case 0x98: { double b = bd(POPL()), a = bd(POPL()); PUSH(a > b ? 1 : a == b ? 0 : a < b ? -1 : (op == 0x98 ? 1 : -1)); break; }
        case 0x84: { int i = code[pc]; locals[i] = (int32_t)((uint32_t)locals[i] + (uint32_t)(int32_t)(int8_t)code[pc + 1]); pc += 2; break; }
        case 0x85: { int32_t a = POP(); PUSHL((int64_t)a); break; }
        case 0x88: { int64_t a = POPL(); PUSH((int32_t)a); break; }
        case 0x91: sp[-1] = (int8_t)sp[-1]; break;
        case 0x92: sp[-1] = (uint16_t)sp[-1]; break;
        case 0x93: sp[-1] = (int16_t)sp[-1]; break;
        case 0x94: { int64_t b = POPL(), a = POPL(); PUSH(a < b ? -1 : a > b ? 1 : 0); break; }
        case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: {
            int32_t v = POP(); int t = op == 0x99 ? v == 0 : op == 0x9a ? v != 0 : op == 0x9b ? v < 0 : op == 0x9c ? v >= 0 : op == 0x9d ? v > 0 : v <= 0;
            if (t) pc = ipc + s16(code + pc); else pc += 2; break; }
        case 0x9f: case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: {
            int32_t b = POP(), a = POP(); int t;
            switch (op) { case 0x9f: case 0xa5: t = a == b; break; case 0xa0: case 0xa6: t = a != b; break; case 0xa1: t = a < b; break; case 0xa2: t = a >= b; break; case 0xa3: t = a > b; break; default: t = a <= b; }
            if (t) pc = ipc + s16(code + pc); else pc += 2; break; }
        case 0xa7: pc = ipc + s16(code + pc); break;
        case 0xc8: pc = ipc + rd32(code + pc); break;
        case 0xc6: if (!POP()) pc = ipc + s16(code + pc); else pc += 2; break;
        case 0xc7: if (POP()) pc = ipc + s16(code + pc); else pc += 2; break;
        case 0xaa: { int32_t v = POP(); uint32_t p = (pc + 3) & ~3u; int32_t def = (int32_t)rd32(code + p), lo = (int32_t)rd32(code + p + 4), hi = (int32_t)rd32(code + p + 8);
            pc = ipc + (v < lo || v > hi ? def : (int32_t)rd32(code + p + 12 + (v - lo) * 4)); break; }
        case 0xab: { int32_t v = POP(); uint32_t p = (pc + 3) & ~3u; int32_t def = (int32_t)rd32(code + p), n = (int32_t)rd32(code + p + 4), off = def;
            for (int k = 0; k < n; k++) if ((int32_t)rd32(code + p + 8 + k * 8) == v) { off = (int32_t)rd32(code + p + 12 + k * 8); break; }
            pc = ipc + off; break; }
        case 0xac: case 0xae: case 0xb0: vm->ret[0] = POP(); return 0;
        case 0xad: case 0xaf: vm->ret[1] = POP(); vm->ret[0] = POP(); return 0;
        case 0xb1: return 0;
        case 0xb2: case 0xb3: case 0xb4: case 0xb5: {
            CP *e = &cls->cp[rd16(code + pc)]; pc += 2; int isstatic = op < 0xb4;
            if (!e->res) {
                Class *rc = vm_find_class(vm, cls->cp[cls->cp[e->a].a].s); if (!rc) THROW("java/lang/NullPointerException");
                Class *own = 0; Field *f = find_field(rc, cls->cp[cls->cp[e->b].a].s, &own); if (!f) THROW("java/lang/NullPointerException");
                e->res = f; e->rcls = own;
            }
            Field *f = (Field *)e->res; int w2 = (f->desc[0] == 'J' || f->desc[0] == 'D');
            if (isstatic) {
                INIT(e->rcls); int32_t *a = e->rcls->statics + f->slot;
                if (op == 0xb2) { PUSH(a[0]); if (w2) PUSH(a[1]); } else { if (w2) a[1] = POP(); a[0] = POP(); }
            } else if (op == 0xb4) { uint32_t r = (uint32_t)POP(); NPE(r); int32_t *a = (int32_t *)(AT(r) + 8) + f->slot; PUSH(a[0]); if (w2) PUSH(a[1]); }
            else { int32_t hi = 0, lo; if (w2) hi = POP(); lo = POP(); uint32_t r = (uint32_t)POP(); NPE(r); int32_t *a = (int32_t *)(AT(r) + 8) + f->slot; a[0] = lo; if (w2) a[1] = hi; }
            break; }
        case 0xb6: case 0xb7: case 0xb8: case 0xb9: {
            CP *e = &cls->cp[rd16(code + pc)]; pc += op == 0xb9 ? 4 : 2; Method *t;
            if (!e->res) {
                Class *rc = vm_find_class(vm, cls->cp[cls->cp[e->a].a].s); if (!rc) THROW("java/lang/NullPointerException");
                const char *nm = cls->cp[cls->cp[e->b].a].s, *ds = cls->cp[cls->cp[e->b].b].s;
                t = vm_find_method(rc, nm, ds); if (!t) THROW("java/lang/NullPointerException");
                e->res = t; e->rcls = rc;
            }
            t = (Method *)e->res; int argsz = t->argsz;
            if (op == 0xb8) { INIT(t->cls); }
            else { uint32_t r = (uint32_t)sp[-argsz]; NPE(r);
                if (op != 0xb7) { Method *v = vm_find_method(obj_class(vm, r), t->name, t->desc); if (v) t = v; } }
            vm->top = sp; int st = call_method(vm, t, sp - argsz); sp -= argsz;
            if (st == -2) THROW("java/lang/StringIndexOutOfBoundsException");
            if (st) { if (st < 0) return st; goto exception; }
            if (t->retsz == 1) PUSH(vm->ret[0]); else if (t->retsz == 2) { PUSH(vm->ret[0]); PUSH(vm->ret[1]); }
            break; }
        case 0xbb: { CP *e = &cls->cp[rd16(code + pc)]; pc += 2;
            if (!e->rcls) { e->rcls = vm_find_class(vm, cls->cp[e->a].s); if (!e->rcls) THROW("java/lang/NullPointerException"); }
            INIT(e->rcls);
            uint32_t r = vm_new_object(vm, e->rcls); if (!r) THROW("java/lang/OutOfMemoryError"); PUSH(r); break; }
        case 0xbc: { int t = code[pc++]; int32_t n = POP(); if (n < 0) THROW("java/lang/NegativeArraySizeException");
            uint32_t r = vm_new_array(vm, t, n); if (!r) THROW("java/lang/OutOfMemoryError"); PUSH(r); break; }
        case 0xbd: { pc += 2; int32_t n = POP(); if (n < 0) THROW("java/lang/NegativeArraySizeException");
            uint32_t r = vm_new_array(vm, 0, n); if (!r) THROW("java/lang/OutOfMemoryError"); PUSH(r); break; }
        case 0xbe: { uint32_t r = (uint32_t)POP(); NPE(r); PUSH(((uint32_t *)AT(r))[1]); break; }
        case 0xbf: { uint32_t r = (uint32_t)POP(); NPE(r); vm->exc = r; goto exception; }
        case 0xc0: case 0xc1: {
            CP *e = &cls->cp[rd16(code + pc)]; pc += 2; uint32_t r = (uint32_t)sp[-1]; int ok = 1;
            if (r) {
                const char *cn = cls->cp[e->a].s; uint32_t id = ((uint32_t *)AT(r))[0];
                if (cn[0] == '[') ok = (id & ARR_FLAG) != 0;
                else { if (!e->rcls) e->rcls = vm_find_class(vm, cn); ok = e->rcls && ((id & ARR_FLAG) ? !strcmp(cn, "java/lang/Object") : is_sub(vm->classes[id], e->rcls)); }
            }
            if (op == 0xc1) { sp[-1] = (r && ok); } else if (!ok) THROW("java/lang/ClassCastException");
            break; }
        case 0xc2: case 0xc3: sp--; break;
        case 0xc4: { uint8_t o2 = code[pc++]; int i = rd16(code + pc); pc += 2;
            switch (o2) { case 0x15: case 0x17: case 0x19: PUSH(locals[i]); break;
                case 0x16: case 0x18: PUSH(locals[i]); PUSH(locals[i + 1]); break;
                case 0x36: case 0x38: case 0x3a: locals[i] = POP(); break;
                case 0x37: case 0x39: locals[i + 1] = POP(); locals[i] = POP(); break;
                case 0x84: locals[i] += s16(code + pc); pc += 2; break; default: return -1; }
            break; }
        case 0xc5: { CP *e = &cls->cp[rd16(code + pc)]; int dims = code[pc + 2]; pc += 3;
            int32_t cnt[8]; if (dims > 2) return -1; for (int k = dims - 1; k >= 0; k--) cnt[k] = POP();
            const char *cn = cls->cp[e->a].s; const char *q = cn + dims; int et = 0;
            if (*q != 'L' && *q != '[') et = (*q == 'B') ? 8 : (*q == 'C') ? 5 : (*q == 'S') ? 9 : (*q == 'Z') ? 4 : (*q == 'J') ? 11 : 10;
            uint32_t outer = vm_new_array(vm, dims > 1 ? 0 : et, cnt[0]); if (!outer) THROW("java/lang/OutOfMemoryError");
            if (dims == 2) for (int k = 0; k < cnt[0]; k++) { uint32_t in = vm_new_array(vm, et, cnt[1]); if (!in) THROW("java/lang/OutOfMemoryError"); ((uint32_t *)(AT(outer) + 8))[k] = in; }
            PUSH(outer); break; }
        default: return -1;
        }
        continue;
exception: {
            uint32_t ex = vm->exc; Class *ec = ex ? obj_class(vm, ex) : 0; int handled = 0;
            for (int k = 0; ec && k < m->nexc && !handled; k++) { Exc *h = &m->exc[k];
                if (ipc >= h->start && ipc < h->end) {
                    int match = !h->catch_type;
                    if (!match) { Class *cc = vm_find_class(vm, cls->cp[cls->cp[h->catch_type].a].s); match = cc && is_sub(ec, cc); }
                    if (match) { sp = sbase; PUSH(ex); vm->exc = 0; pc = h->handler; handled = 1; } } }
            if (!handled) return 1;
        }
    }
}
