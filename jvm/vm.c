/* CLDC-style JVM core for the Fit3 watch (see vm.h). Interpreter, conservative mark/sweep GC, cooperative green threads. */
#include "sys.h"
#include "vm.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define ACC_STATIC 0x0008
#define ACC_NATIVE 0x0100
#define ARR_FLAG 0x80000000u
#define FREEBLK 0x7ffffff0u
#define MAX_DEPTH 90
#define FUEL 20000
#define CSTACK 24576
#define JSTACK_OTHER 6144

typedef struct { uint16_t start, end, handler, catch_type; } Exc;
typedef struct { uint8_t tag; uint16_t a, b; uint32_t u; void *p1, *p2; } CP;      /* p1: Utf8 text / resolved Field or Method; p2: resolved class */
typedef struct { Method *m; int32_t cid; } ICE;
typedef struct { const char *name, *desc; uint16_t acc; int slot; uint16_t cv; } Field;
struct Method {
    const char *name, *desc; uint16_t acc, max_stack, max_locals; const uint8_t *code; uint32_t code_len;
    Exc *exc; uint16_t nexc; NativeFn native; int argsz, retsz; Class *cls;
};
struct Class {
    const char *name; Class *super; Class **ifaces; int niface; CP *cp; int ncp; Field *fields; int nfields;
    Method *methods; int nmethods; int nslots; int32_t *statics; int nstatics; int state, id; uint16_t acc; ICE *ic;
};
#define UTF(c, i) ((const char *)(c)->cp[i].p1)

/* ---------- helpers ---------- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int16_t s16(const uint8_t *p) { return (int16_t)rd16(p); }
static char *dups(const char *s) { size_t n = strlen(s); char *r = (char *)malloc(n + 1); memcpy(r, s, n + 1); return r; }

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
static const BClass bclasses[] = { {"java/lang/Object", 0, 0} };
static int n_nop(VM *vm, int32_t *a) { (void)vm; (void)a; return 0; }
static int n_obj_equals(VM *vm, int32_t *a) { vm->ret[0] = a[0] == a[1]; return 0; }
static int n_obj_hash(VM *vm, int32_t *a) { vm->ret[0] = a[0]; return 0; }
static const NativeEntry builtin_natives[] = {
    {"java/lang/Object", "equals", "(Ljava/lang/Object;)Z", n_obj_equals, 0},
    {"java/lang/Object", "hashCode", "()I", n_obj_hash, 0},
    {"java/lang/Object", "<init>", "()V", n_nop, 0},
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

/* ---------- heap + GC ---------- */
static int elem_size(int t) { switch (t) { case 4: case 8: return 1; case 5: case 9: return 2; case 11: case 7: return 8; default: return 4; } }
#define W(vm, r, i) (((uint32_t *)((vm)->heap + (r)))[i])
static uint32_t round8(uint32_t n) { return (n + 7) & ~7u; }
static uint32_t blk_size(VM *vm, uint32_t r) {
    uint32_t w0 = W(vm, r, 0);
    if (w0 == FREEBLK) return W(vm, r, 1);
    if (w0 & ARR_FLAG) return round8(8 + W(vm, r, 1) * (uint32_t)elem_size((int)(w0 & 0xff)));
    return round8(8 + (uint32_t)vm->classes[w0]->nslots * 4);
}
#define BIT(arr, r) ((arr)[(r) >> 8] & (1u << (((r) >> 3) & 31)))
#define SETBIT(arr, r) ((arr)[(r) >> 8] |= (1u << (((r) >> 3) & 31)))
#define CLRBIT(arr, r) ((arr)[(r) >> 8] &= ~(1u << (((r) >> 3) & 31)))

#define NBIN 17                                  /* size classes 16, 24, ... 144 */
static void free_push(VM *vm, uint32_t r, uint32_t bs) {
    W(vm, r, 0) = FREEBLK; W(vm, r, 1) = bs;
    if (bs < 16) return;                         /* 8-byte dust: stays walkable, reused after coalescing */
    if (bs <= 16 + 8 * (NBIN - 1)) { uint32_t *h = &vm->bins[(bs - 16) / 8]; W(vm, r, 2) = *h; *h = r; }
    else { W(vm, r, 2) = vm->free_head; vm->free_head = r; }
}
static uint32_t heap_alloc(VM *vm, uint32_t bytes) {
    uint32_t sz = round8(bytes), r = 0, bs = 0;
    if (bytes > vm->heap_size) return 0;
    if (sz <= 16 + 8 * (NBIN - 1)) {
        for (int j = sz < 16 ? 0 : (int)(sz - 16) / 8; j < NBIN; j++) if (vm->bins[j]) { r = vm->bins[j]; vm->bins[j] = W(vm, r, 2); bs = 16 + 8 * (uint32_t)j; break; }
    }
    if (!r) {
        uint32_t prev = 0, cur = vm->free_head;
        while (cur) { uint32_t b2 = W(vm, cur, 1), next = W(vm, cur, 2);
            if (b2 >= sz) { if (prev) W(vm, prev, 2) = next; else vm->free_head = next; r = cur; bs = b2; break; }
            prev = cur; cur = next; }
    }
    if (r) { if (bs > sz) free_push(vm, r + sz, bs - sz); else sz = bs; }
    else { if (vm->heap_top + sz > vm->heap_size) return 0; r = vm->heap_top; vm->heap_top += sz; }
    memset(vm->heap + r, 0, sz); SETBIT(vm->startbits, r); vm->used += sz; return r;
}
uint32_t vm_new_array(VM *vm, int type, int32_t len) {
    if (len < 0 || (uint32_t)len > vm->heap_size) return 0;
    uint32_t r = heap_alloc(vm, 8 + (uint32_t)len * (uint32_t)elem_size(type)); if (!r) return 0;
    W(vm, r, 0) = ARR_FLAG | (uint32_t)type; W(vm, r, 1) = (uint32_t)len; return r;
}
uint32_t vm_new_object(VM *vm, Class *c) {
    uint32_t r = heap_alloc(vm, 8 + (uint32_t)c->nslots * 4); if (!r) return 0;
    W(vm, r, 0) = (uint32_t)c->id; return r;
}
uint32_t vm_new_string(VM *vm, const char *s) {
    int n = (int)strlen(s); uint32_t arr = vm_new_array(vm, 5, n); if (!arr) return 0;
    for (int i = 0; i < n; i++) ((uint16_t *)(vm->heap + arr + 8))[i] = (uint8_t)s[i];
    if (!vm->c_string) vm->c_string = vm_find_class(vm, "java/lang/String");
    if (!vm->c_string) return 0;
    uint32_t o = vm_new_object(vm, vm->c_string);
    if (o) ((int32_t *)(vm->heap + o + 8))[0] = (int32_t)arr; return o;
}
void vm_pin(VM *vm, uint32_t *slot) { if (vm->npins < VM_MAX_PINS) vm->pins[vm->npins++] = slot; }

static void mark_push(VM *vm, uint32_t r) {
    if (vm->marksp < vm->markcap) vm->markstack[vm->marksp++] = r; else vm->markover = 1;
}
static void mark_value(VM *vm, uint32_t v) {
    if (v < 16 || (v & 7) || v >= vm->heap_top || !BIT(vm->startbits, v) || BIT(vm->markbits, v)) return;
    SETBIT(vm->markbits, v); mark_push(vm, v);
}
static void mark_children(VM *vm, uint32_t r) {
    uint32_t w0 = W(vm, r, 0);
    if (w0 == FREEBLK) return;
    if (w0 & ARR_FLAG) { if ((w0 & 0xff) == 0) { uint32_t n = W(vm, r, 1); for (uint32_t i = 0; i < n; i++) mark_value(vm, W(vm, r, 2 + i)); } }
    else { int n = vm->classes[w0]->nslots; for (int i = 0; i < n; i++) mark_value(vm, W(vm, r, 2 + (uint32_t)i)); }
}
static void mark_drain(VM *vm) {
    while (vm->marksp) { uint32_t r = vm->markstack[--vm->marksp]; mark_children(vm, r); }
    while (vm->markover) {                      /* stack overflowed: rescan the heap for marked objects until stable */
        vm->markover = 0;
        for (uint32_t r = 16; r < vm->heap_top; ) { uint32_t bs = blk_size(vm, r); if (BIT(vm->markbits, r)) { mark_children(vm, r); while (vm->marksp) mark_children(vm, vm->markstack[--vm->marksp]); } r += bs; }
    }
}
void vm_gc(VM *vm) {
    uint32_t nw = (vm->heap_size >> 8) + 1; int i, k;
    vm->ngc++;
    for (uint32_t j = 0; j < nw; j++) vm->markbits[j] = 0;
    vm->marksp = 0; vm->markover = 0;
    for (i = 0; i < vm->npins; i++) mark_value(vm, *vm->pins[i]);
    mark_value(vm, vm->exc);
    for (i = 0; i < VM_MAX_THREADS; i++) {
        JThread *t = &vm->threads[i]; if (t->state == 0 || t->state == 3) continue;
        int32_t *top = (t == vm->cur) ? vm->top : t->top; if (!top) continue;
        for (int32_t *p = t->stack; p < top; p++) mark_value(vm, (uint32_t)*p);
        mark_value(vm, t->obj); mark_value(vm, t->waiton); mark_value(vm, t->joinon);
        for (k = 0; k < t->nsargs; k++) mark_value(vm, (uint32_t)t->sargs[k]);
    }
    for (i = 0; i < vm->nclasses; i++) {
        Class *c = vm->classes[i];
        for (k = 0; k < c->nstatics; k++) mark_value(vm, (uint32_t)c->statics[k]);
        for (k = 1; k < c->ncp; k++) if (c->cp[k].tag == 8) mark_value(vm, c->cp[k].u);
        if (i % 16 == 0) mark_drain(vm);
    }
    mark_drain(vm);
    /* sweep: free unmarked blocks, coalesce, rebuild the free list; give a free tail back to the bump region */
    vm->used = 0;
    uint32_t tail = 0;                                         /* start of the trailing free run */
    uint32_t r = 16, run = 0, runsz = 0;
    for (; r < vm->heap_top; ) {
        uint32_t bs = blk_size(vm, r);
        int live = BIT(vm->startbits, r) && BIT(vm->markbits, r);
        if (live) {
            if (run) { W(vm, run, 0) = FREEBLK; W(vm, run, 1) = runsz; run = 0; }
            vm->used += bs;
        } else {
            CLRBIT(vm->startbits, r);
            if (!run) { run = r; runsz = 0; }
            runsz += bs;
        }
        /* finalize free run when next block is live or at the end */
        r += bs;
        if (run && (r >= vm->heap_top || BIT(vm->markbits, r))) {
            if (r >= vm->heap_top) { tail = run; vm->heap_top = run; run = 0; }
            else { W(vm, run, 0) = FREEBLK; W(vm, run, 1) = runsz; run = 0; }
        }
    }
    (void)tail;
    /* second pass: put the free blocks (>= 16 bytes) into the size bins / the address-ordered big list */
    { for (int j = 0; j < NBIN; j++) vm->bins[j] = 0; vm->free_head = 0; uint32_t *lastnext = &vm->free_head;
      for (r = 16; r < vm->heap_top; ) { uint32_t bs = blk_size(vm, r);
          if (W(vm, r, 0) == FREEBLK && bs >= 16) {
              if (bs <= 16 + 8 * (NBIN - 1)) { uint32_t *h = &vm->bins[(bs - 16) / 8]; W(vm, r, 2) = *h; *h = r; }
              else { W(vm, r, 2) = 0; *lastnext = r; lastnext = &W(vm, r, 2); } }
          r += bs; } }
    { uint32_t room = vm->heap_size - vm->used; vm->gc_at = vm->used + room / 2; if (vm->gc_at + 12288 > vm->heap_size) vm->gc_at = vm->heap_size > 12288 ? vm->heap_size - 12288 : 0; if (vm->gc_at < vm->used + 4096) vm->gc_at = vm->used + 4096; }
}
static void gc_check(VM *vm, int32_t *sp) { if (vm->used >= vm->gc_at) { vm->top = sp; vm_gc(vm); } }

/* ---------- class loading ---------- */
static Class *add_class(VM *vm, Class *c) {
    if (vm->nclasses == vm->cap_classes) { vm->cap_classes = vm->cap_classes ? vm->cap_classes * 2 : 64;
        vm->classes = (Class **)realloc(vm->classes, vm->cap_classes * sizeof(Class *)); }
    c->id = vm->nclasses; vm->classes[vm->nclasses++] = c; return c;
}
static Class *make_builtin(VM *vm, const BClass *b) {
    Class *c = (Class *)calloc(1, sizeof(Class)); c->name = b->name;
    c->super = b->super ? vm_find_class(vm, b->super) : 0;
    c->nslots = (c->super ? c->super->nslots : 0) + b->nfields; c->state = 2;
    int cnt = 0, i;
    for (i = 0; i < NB; i++) if (!strcmp(builtin_natives[i].cls, b->name)) cnt++;
    for (i = 0; i < vm->nnatives; i++) if (!strcmp(vm->natives[i].cls, b->name)) cnt++;
    c->methods = (Method *)calloc(cnt ? cnt : 1, sizeof(Method));
    for (i = 0; i < NB + vm->nnatives; i++) {
        const NativeEntry *e = i < NB ? &builtin_natives[i] : &vm->natives[i - NB];
        if (strcmp(e->cls, b->name)) continue;
        Method *m = &c->methods[c->nmethods++]; m->name = e->name; m->desc = e->desc; m->native = e->fn;
        m->acc = (uint16_t)(e->is_static ? (ACC_STATIC | ACC_NATIVE) : ACC_NATIVE); m->cls = c;
        desc_sizes(m->desc, &m->argsz, &m->retsz); if (!e->is_static) m->argsz++;
    }
    return add_class(vm, c);
}
static Class *parse_class(VM *vm, const uint8_t *b) {
    if (rd32(b) != 0xCAFEBABE) return 0;
    int ncp = rd16(b + 8), i; const uint8_t *p = b + 10;
    /* pass 1: size of the Utf8 arena */
    uint32_t ar = 0; { const uint8_t *q = p; for (i = 1; i < ncp; i++) { uint8_t t = *q++;
        switch (t) { case 1: { int n = rd16(q); ar += (uint32_t)n + 1; q += 2 + n; break; } case 3: case 4: q += 4; break;
            case 5: case 6: q += 8; i++; break; case 7: case 8: q += 2; break; case 9: case 10: case 11: case 12: q += 4; break;
            case 15: q += 3; break; case 16: q += 2; break; case 18: q += 4; break; default: return 0; } } }
    char *arena = (char *)malloc(ar + 1), *aw = arena;
    Class *c = (Class *)calloc(1, sizeof(Class)); c->ncp = ncp; c->cp = (CP *)calloc(ncp, sizeof(CP));
    for (i = 1; i < ncp; i++) {
        CP *e = &c->cp[i]; e->tag = *p++;
        switch (e->tag) {
        case 1: { int n = rd16(p); memcpy(aw, p + 2, n); aw[n] = 0; e->p1 = aw; aw += n + 1; p += 2 + n; break; }
        case 3: case 4: e->u = rd32(p); p += 4; break;
        case 5: case 6: e->u = rd32(p + 4); c->cp[i + 1].u = rd32(p); p += 8; i++; break;     /* low word here, high word in the next entry */
        case 7: case 8: e->a = rd16(p); p += 2; break;
        case 9: case 10: case 11: case 12: e->a = rd16(p); e->b = rd16(p + 2); p += 4; break;
        case 15: p += 3; break; case 16: p += 2; break; case 18: p += 4; break;
        default: return 0; }
    }
    c->acc = rd16(p); c->name = UTF(c, c->cp[rd16(p + 2)].a);
    uint16_t sup = rd16(p + 4); c->niface = rd16(p + 6); p += 8;
    c->super = sup ? vm_find_class(vm, UTF(c, c->cp[sup].a)) : 0;
    c->ifaces = (Class **)calloc(c->niface ? c->niface : 1, sizeof(Class *));
    for (i = 0; i < c->niface; i++, p += 2) c->ifaces[i] = vm_find_class(vm, UTF(c, c->cp[rd16(p)].a));
    c->nslots = c->super ? c->super->nslots : 0;
    c->nfields = rd16(p); p += 2; c->fields = (Field *)calloc(c->nfields ? c->nfields : 1, sizeof(Field));
    for (i = 0; i < c->nfields; i++) {
        Field *f = &c->fields[i]; f->acc = rd16(p); f->name = UTF(c, rd16(p + 2)); f->desc = UTF(c, rd16(p + 4));
        int w = (f->desc[0] == 'J' || f->desc[0] == 'D') ? 2 : 1;
        if (f->acc & ACC_STATIC) { f->slot = c->nstatics; c->nstatics += w; } else { f->slot = c->nslots; c->nslots += w; }
        int na = rd16(p + 6); p += 8;
        while (na--) { if (!strcmp(UTF(c, rd16(p)), "ConstantValue")) f->cv = rd16(p + 6); p += 6 + rd32(p + 2); }
    }
    c->statics = (int32_t *)calloc(c->nstatics ? c->nstatics : 1, 4);
    c->nmethods = rd16(p); p += 2; c->methods = (Method *)calloc(c->nmethods ? c->nmethods : 1, sizeof(Method));
    for (i = 0; i < c->nmethods; i++) {
        Method *m = &c->methods[i]; m->acc = rd16(p); m->name = UTF(c, rd16(p + 2)); m->desc = UTF(c, rd16(p + 4)); m->cls = c;
        desc_sizes(m->desc, &m->argsz, &m->retsz); if (!(m->acc & ACC_STATIC)) m->argsz++;
        int na = rd16(p + 6); p += 8;
        while (na--) {
            const char *an = UTF(c, rd16(p)); uint32_t al = rd32(p + 2); const uint8_t *q = p + 6;
            if (!strcmp(an, "Code")) {
                m->max_stack = rd16(q); m->max_locals = rd16(q + 2); m->code_len = rd32(q + 4); m->code = q + 8;
                const uint8_t *e = q + 8 + m->code_len; m->nexc = rd16(e); e += 2;
                m->exc = (Exc *)calloc(m->nexc ? m->nexc : 1, sizeof(Exc));
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
int vm_field_slot(Class *c, const char *name) {
    for (; c; c = c->super) for (int i = 0; i < c->nfields; i++) if (!(c->fields[i].acc & ACC_STATIC) && !strcmp(c->fields[i].name, name)) return c->fields[i].slot;
    return -1;
}
static Field *find_field(Class *c, const char *name, const char *desc, Class **owner) {
    for (; c; c = c->super) {
        for (int i = 0; i < c->nfields; i++) if (!strcmp(c->fields[i].name, name) && !strcmp(c->fields[i].desc, desc)) { *owner = c; return &c->fields[i]; }
        for (int i = 0; i < c->niface; i++) { Field *f = find_field(c->ifaces[i], name, desc, owner); if (f) return f; }
    }
    return 0;
}
static int is_sub(Class *c, Class *t) {
    for (; c; c = c->super) { if (c == t) return 1; for (int i = 0; i < c->niface; i++) if (is_sub(c->ifaces[i], t)) return 1; }
    return 0;
}
static Class *obj_class(VM *vm, uint32_t r) { uint32_t id = W(vm, r, 0); if (id & ARR_FLAG) { if (!vm->c_object) vm->c_object = vm_find_class(vm, "java/lang/Object"); return vm->c_object; } return vm->classes[id]; }
int vm_instance_of(VM *vm, uint32_t ref, const char *cls) { Class *t = vm_find_class(vm, cls); return ref && t && is_sub(obj_class(vm, ref), t); }

/* ---------- float helpers ---------- */
static float bf(int32_t v) { float f; memcpy(&f, &v, 4); return f; }
static int32_t fb(float f) { int32_t v; memcpy(&v, &f, 4); return v; }
static double bd(int64_t v) { double d; memcpy(&d, &v, 8); return d; }
static int64_t db(double d) { int64_t v; memcpy(&v, &d, 8); return v; }
static int32_t d2i(double d) { return d != d ? 0 : d >= 2147483647.0 ? 2147483647 : d <= -2147483648.0 ? (-2147483647 - 1) : (int32_t)d; }
static int64_t d2l(double d) { return d != d ? 0 : d >= 9.2233720368547758e18 ? 0x7fffffffffffffffLL : d <= -9.2233720368547758e18 ? (-0x7fffffffffffffffLL - 1) : (int64_t)d; }

/* ---------- coroutines (one C stack per Java thread) ---------- */
static VM *g_vm;
#if defined(F3APP) || defined(__arm__)
__attribute__((naked, noinline)) static void co_switch(void **save, void *to) {
    __asm__ volatile("push {r4-r11, lr}\n\tvpush {s16-s31}\n\tstr sp, [r0]\n\tmov sp, r1\n\tvpop {s16-s31}\n\tpop {r4-r11, pc}\n\t");
}
static void thread_main(void);
static int co_create(VM *vm, JThread *t) {
    t->cstack = (uint8_t *)malloc(CSTACK + 8); if (!t->cstack) return -1; t->cstack_size = CSTACK;
    uint32_t top = ((uint32_t)(uintptr_t)t->cstack + CSTACK) & ~7u; uint32_t *sp = (uint32_t *)(uintptr_t)(top - 25 * 4);
    for (int i = 0; i < 24; i++) sp[i] = 0;
    sp[24] = (uint32_t)(uintptr_t)thread_main | 1u; t->ctx = sp; (void)vm; return 0;
}
static void co_destroy(JThread *t) { if (t->cstack) free(t->cstack); t->cstack = 0; t->ctx = 0; }
static void co_resume(VM *vm, JThread *t) { co_switch(&vm->sched_ctx, t->ctx); }
static void co_suspend(VM *vm, JThread *t) { co_switch(&t->ctx, vm->sched_ctx); }
#elif defined(_WIN32)
static void thread_main(void);
static VOID CALLBACK fiber_entry(LPVOID p) { (void)p; thread_main(); }
static int co_create(VM *vm, JThread *t) { (void)vm; t->ctx = CreateFiber(256 * 1024, fiber_entry, t); return t->ctx ? 0 : -1; }
static void co_destroy(JThread *t) { if (t->ctx) DeleteFiber(t->ctx); t->ctx = 0; }
static void co_resume(VM *vm, JThread *t) { if (!vm->sched_ctx) vm->sched_ctx = ConvertThreadToFiber(0); SwitchToFiber(t->ctx); }
static void co_suspend(VM *vm, JThread *t) { (void)t; SwitchToFiber(vm->sched_ctx); }
#else
#error no coroutine implementation for this platform
#endif

/* ---------- interpreter ---------- */
static int exec(VM *vm, Method *m, int32_t *locals);
static int call_method(VM *vm, Method *m, int32_t *args) {
    if (m->native) return m->native(vm, args);
    if (!m->code) { vm->err = m->name; vm->err_m = m; return -1; }
    if (vm->depth > MAX_DEPTH) { vm_throw(vm, "java/lang/StackOverflowError"); return 1; }
    vm->depth++; int r = exec(vm, m, args); vm->depth--; return r;
}
/* caller must have set vm->top to the first free stack slot */
static int init_class(VM *vm, Class *c) {
    if (c->state) return 0; c->state = 1;
    if (c->super && init_class(vm, c->super)) return 1;
    for (int i = 0; i < c->nfields; i++) { Field *f = &c->fields[i]; if (!f->cv || !(f->acc & ACC_STATIC)) continue;
        CP *e = &c->cp[f->cv]; int32_t *s = c->statics + f->slot;
        if (e->tag == 8) s[0] = (int32_t)vm_new_string(vm, UTF(c, e->a));
        else if (e->tag == 5 || e->tag == 6) { s[0] = (int32_t)e->u; s[1] = (int32_t)c->cp[f->cv + 1].u; } else s[0] = (int32_t)e->u; }
    Method *cl = 0; for (int i = 0; i < c->nmethods; i++) if (!strcmp(c->methods[i].name, "<clinit>")) cl = &c->methods[i];
    if (cl) { int32_t *base = vm->top; vm->top = base + cl->max_locals + cl->max_stack + 2; int r = call_method(vm, cl, base); vm->top = base; if (r) return 1; }
    return 0;
}
static int throw_new(VM *vm, const char *cn) { Class *c = vm_find_class(vm, cn); if (!c) vm->err = cn; vm->exc = c ? vm_new_object(vm, c) : 0; return 1; }
int vm_throw(VM *vm, const char *cn) { return throw_new(vm, cn); }
static int throw_msg(VM *vm, const char *cn, const char *a, const char *b) {          /* exception with message a + b (no GC can happen here) */
    char buf[160]; size_t n = 0; while (a && a[n] && n < 100) { buf[n] = a[n]; n++; }
    if (b) { size_t k = 0; buf[n++] = '.'; while (b[k] && n < 150) buf[n++] = b[k++]; }
    buf[n] = 0; throw_new(vm, cn);
    if (vm->exc) { uint32_t s = vm_new_string(vm, buf); if (s) { W(vm, vm->exc, 2) = s; } }
    return 1;
}
Class *vm_obj_class(VM *vm, uint32_t r) { return obj_class(vm, r); }
int vm_class_id(Class *c) { return c->id; }
const char *vm_method_class(Method *m) { return m->cls->name; }
const char *vm_method_name(Method *m) { return m->name; }
const char *vm_exc_name(VM *vm) { return vm->exc ? obj_class(vm, vm->exc)->name : "(none)"; }

#define PUSH(x) (*sp++ = (int32_t)(x))
#define POP() (*--sp)
#define PUSHL(x) do { int64_t _v = (x); sp[0] = (int32_t)_v; sp[1] = (int32_t)(_v >> 32); sp += 2; } while (0)
static int64_t popl(int32_t **spp) { int32_t *sp = *spp - 2; *spp = sp; return (int64_t)((uint64_t)(uint32_t)sp[0] | ((uint64_t)(uint32_t)sp[1] << 32)); }
#define POPL() popl(&sp)
#define AT(r) ((uint8_t *)(vm->heap + (r)))
#define THROW(name) do { throw_new(vm, name); goto exception; } while (0)
#define THROWM(name, a, b) do { throw_msg(vm, name, a, b); goto exception; } while (0)
#define NPE(r) do { if (!(r)) THROW("java/lang/NullPointerException"); } while (0)
#define ACHK(r, i) do { NPE(r); if ((uint32_t)(i) >= ((uint32_t *)AT(r))[1]) THROW("java/lang/ArrayIndexOutOfBoundsException"); } while (0)
#define INIT(c) do { vm->top = sp; if (init_class(vm, (c))) goto exception; } while (0)
#define FUELCHK() do { if (--vm->fuel < 0) { vm->fuel = FUEL; if (vm->cur) { vm->top = sp; vm_yield(vm, 0); } } } while (0)
#define ALLOC_RETRY(expr, res) do { res = (expr); if (!res) { vm->top = sp; vm_gc(vm); res = (expr); } if (!res) THROW("java/lang/OutOfMemoryError"); } while (0)

static int exec(VM *vm, Method *m, int32_t *locals) {
    Class *cls = m->cls; const uint8_t *code = m->code; uint32_t pc = 0, ipc = 0;
    int32_t *sbase = locals + m->max_locals, *sp = sbase; int32_t tmp;
    if (sbase + m->max_stack + 2 > vm->stack + vm->stack_size) { vm_throw(vm, "java/lang/StackOverflowError"); return 1; }
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
            if (e->tag == 8) { if (!e->u) { vm->top = sp; uint32_t s = vm_new_string(vm, UTF(cls, e->a)); if (!s) { vm_gc(vm); s = vm_new_string(vm, UTF(cls, e->a)); } if (!s) THROW("java/lang/OutOfMemoryError"); e->u = s; } PUSH(e->u); }
            else PUSH(e->u);
            break; }
        case 0x14: { int idx = rd16(code + pc); pc += 2; PUSH(cls->cp[idx].u); PUSH(cls->cp[idx + 1].u); break; }
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
            if (t) { int16_t off = s16(code + pc); pc = ipc + off; if (off < 0) FUELCHK(); } else pc += 2; break; }
        case 0x9f: case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: {
            int32_t b = POP(), a = POP(); int t;
            switch (op) { case 0x9f: case 0xa5: t = a == b; break; case 0xa0: case 0xa6: t = a != b; break; case 0xa1: t = a < b; break; case 0xa2: t = a >= b; break; case 0xa3: t = a > b; break; default: t = a <= b; }
            if (t) { int16_t off = s16(code + pc); pc = ipc + off; if (off < 0) FUELCHK(); } else pc += 2; break; }
        case 0xa7: { int16_t off = s16(code + pc); pc = ipc + off; if (off < 0) FUELCHK(); break; }
        case 0xc8: pc = ipc + rd32(code + pc); FUELCHK(); break;
        case 0xa8: PUSH(pc + 2); pc = ipc + s16(code + pc); break;                       /* jsr */
        case 0xc9: PUSH(pc + 4); pc = ipc + rd32(code + pc); break;                      /* jsr_w */
        case 0xa9: pc = (uint32_t)locals[code[pc]]; break;                               /* ret */
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
            if (!e->p1) {
                Class *rc = vm_find_class(vm, UTF(cls, cls->cp[e->a].a)); if (!rc) THROWM("java/lang/NoClassDefFoundError", UTF(cls, cls->cp[e->a].a), 0);
                Class *own = 0; Field *f = find_field(rc, UTF(cls, cls->cp[e->b].a), UTF(cls, cls->cp[e->b].b), &own); if (!f) THROWM("java/lang/NoSuchFieldError", UTF(cls, cls->cp[e->a].a), UTF(cls, cls->cp[e->b].a));
                e->p1 = f; e->p2 = own;
            }
            Field *f = (Field *)e->p1; int w2 = (f->desc[0] == 'J' || f->desc[0] == 'D');
            if (isstatic) {
                INIT((Class *)e->p2); int32_t *a = ((Class *)e->p2)->statics + f->slot;
                if (op == 0xb2) { PUSH(a[0]); if (w2) PUSH(a[1]); } else { if (w2) a[1] = POP(); a[0] = POP(); }
            } else if (op == 0xb4) { uint32_t r = (uint32_t)POP(); NPE(r); int32_t *a = (int32_t *)(AT(r) + 8) + f->slot; PUSH(a[0]); if (w2) PUSH(a[1]); }
            else { int32_t hi = 0, lo; if (w2) hi = POP(); lo = POP(); uint32_t r = (uint32_t)POP(); NPE(r); int32_t *a = (int32_t *)(AT(r) + 8) + f->slot; a[0] = lo; if (w2) a[1] = hi; }
            break; }
        case 0xb6: case 0xb7: case 0xb8: case 0xb9: {
            uint16_t idx = rd16(code + pc); CP *e = &cls->cp[idx]; pc += op == 0xb9 ? 4 : 2; Method *t;
            if (!e->p1) {
                Class *rc = vm_find_class(vm, UTF(cls, cls->cp[e->a].a)); if (!rc) THROWM("java/lang/NoClassDefFoundError", UTF(cls, cls->cp[e->a].a), 0);
                const char *nm = UTF(cls, cls->cp[e->b].a), *ds = UTF(cls, cls->cp[e->b].b);
                t = vm_find_method(rc, nm, ds); if (!t) THROWM("java/lang/NoSuchMethodError", UTF(cls, cls->cp[e->a].a), nm);
                e->p1 = t;
            }
            t = (Method *)e->p1; int argsz = t->argsz;
            if (op == 0xb8) { INIT(t->cls); }
            else { uint32_t r = (uint32_t)sp[-argsz]; NPE(r);
                if (op != 0xb7) {
                    Class *rc = obj_class(vm, r);
                    if (!cls->ic) cls->ic = (ICE *)calloc(cls->ncp, sizeof(ICE));
                    ICE *ic = &cls->ic[idx];
                    if (ic->cid == rc->id + 1) t = ic->m;
                    else { Method *v = vm_find_method(rc, t->name, t->desc); if (v) t = v; ic->cid = rc->id + 1; ic->m = t; }
                } }
            FUELCHK();
            vm->top = sp;
            if (t->native && vm->used >= vm->gc_at) vm_gc(vm);
            int st = call_method(vm, t, sp - argsz); sp -= argsz;
            if (st == -2) THROW("java/lang/StringIndexOutOfBoundsException");
            if (st) { if (st < 0) return st; goto exception; }
            if (t->retsz == 1) PUSH(vm->ret[0]); else if (t->retsz == 2) { PUSH(vm->ret[0]); PUSH(vm->ret[1]); }
            break; }
        case 0xbb: { CP *e = &cls->cp[rd16(code + pc)]; pc += 2;
            if (!e->p2) { e->p2 = vm_find_class(vm, UTF(cls, e->a)); if (!e->p2) THROWM("java/lang/NoClassDefFoundError", UTF(cls, e->a), 0); }
            INIT((Class *)e->p2);
            uint32_t r; if (vm->used >= vm->gc_at) { vm->top = sp; vm_gc(vm); }
            ALLOC_RETRY(vm_new_object(vm, (Class *)e->p2), r); PUSH(r); break; }
        case 0xbc: { int t = code[pc++]; int32_t n = POP(); if (n < 0) THROW("java/lang/NegativeArraySizeException");
            uint32_t r; if (vm->used >= vm->gc_at) { vm->top = sp; vm_gc(vm); }
            ALLOC_RETRY(vm_new_array(vm, t, n), r); PUSH(r); break; }
        case 0xbd: { pc += 2; int32_t n = POP(); if (n < 0) THROW("java/lang/NegativeArraySizeException");
            uint32_t r; if (vm->used >= vm->gc_at) { vm->top = sp; vm_gc(vm); }
            ALLOC_RETRY(vm_new_array(vm, 0, n), r); PUSH(r); break; }
        case 0xbe: { uint32_t r = (uint32_t)POP(); NPE(r); PUSH(((uint32_t *)AT(r))[1]); break; }
        case 0xbf: { uint32_t r = (uint32_t)POP(); NPE(r); vm->exc = r; goto exception; }
        case 0xc0: case 0xc1: {
            CP *e = &cls->cp[rd16(code + pc)]; pc += 2; uint32_t r = (uint32_t)sp[-1]; int ok = 1;
            if (r) {
                const char *cn = UTF(cls, e->a); uint32_t id = ((uint32_t *)AT(r))[0];
                if (cn[0] == '[') ok = (id & ARR_FLAG) != 0;
                else { if (!e->p2) e->p2 = vm_find_class(vm, cn); ok = e->p2 && ((id & ARR_FLAG) ? !strcmp(cn, "java/lang/Object") : is_sub(vm->classes[id], (Class *)e->p2)); }
            }
            if (op == 0xc1) { sp[-1] = (r && ok); } else if (!ok) THROW("java/lang/ClassCastException");
            break; }
        case 0xc2: case 0xc3: sp--; break;
        case 0xc4: { uint8_t o2 = code[pc++]; int i = rd16(code + pc); pc += 2;
            switch (o2) { case 0x15: case 0x17: case 0x19: PUSH(locals[i]); break;
                case 0x16: case 0x18: PUSH(locals[i]); PUSH(locals[i + 1]); break;
                case 0x36: case 0x38: case 0x3a: locals[i] = POP(); break;
                case 0x37: case 0x39: locals[i + 1] = POP(); locals[i] = POP(); break;
                case 0x84: locals[i] += s16(code + pc); pc += 2; break;
                case 0xa9: pc = (uint32_t)locals[i]; break;
                default: vm->err = "bad wide opcode"; return -1; }
            break; }
        case 0xc5: { CP *e = &cls->cp[rd16(code + pc)]; int dims = code[pc + 2]; pc += 3;
            int32_t cnt[4]; if (dims > 4) { vm->err = "multianewarray too deep"; return -1; } for (int k = dims - 1; k >= 0; k--) cnt[k] = POP();
            for (int k = 0; k < dims; k++) if (cnt[k] < 0) THROW("java/lang/NegativeArraySizeException");
            const char *cn = UTF(cls, e->a); const char *q = cn + dims; int et = 0;
            if (*q != 'L' && *q != '[') et = (*q == 'B') ? 8 : (*q == 'C') ? 5 : (*q == 'S') ? 9 : (*q == 'Z') ? 4 : (*q == 'J') ? 11 : (*q == 'F') ? 6 : (*q == 'D') ? 7 : 10;
            uint32_t outer; if (vm->used >= vm->gc_at) { vm->top = sp; vm_gc(vm); }
            ALLOC_RETRY(vm_new_array(vm, dims > 1 ? 0 : et, cnt[0]), outer);
            PUSH(outer);                                  /* keep it reachable (conservative stack) while the inner arrays are built */
            if (dims >= 2) for (int k = 0; k < cnt[0]; k++) {
                uint32_t in; ALLOC_RETRY(vm_new_array(vm, dims > 2 ? 0 : et, cnt[1]), in);
                ((uint32_t *)(AT(outer) + 8))[k] = in;
                if (dims == 3) { PUSH(in); for (int j = 0; j < cnt[1]; j++) { uint32_t in2; ALLOC_RETRY(vm_new_array(vm, et, cnt[2]), in2); ((uint32_t *)(AT(in) + 8))[j] = in2; } sp--; }
            }
            break; }
        default: vm->err = "unsupported opcode"; return -1;
        }
        continue;
exception: {
            uint32_t ex = vm->exc; Class *ec = ex ? obj_class(vm, ex) : 0; int handled = 0;
            for (int k = 0; ec && k < m->nexc && !handled; k++) { Exc *h = &m->exc[k];
                if (ipc >= h->start && ipc < h->end) {
                    int match = !h->catch_type;
                    if (!match) { CP *ce = &cls->cp[h->catch_type]; if (!ce->p2) ce->p2 = vm_find_class(vm, UTF(cls, ce->a)); match = ce->p2 && is_sub(ec, (Class *)ce->p2); }
                    if (match) { sp = sbase; PUSH(ex); vm->exc = 0; pc = h->handler; handled = 1; } } }
            if (!handled) { if (vm->ntrace < 12) { vm->tr_m[vm->ntrace] = m; vm->tr_pc[vm->ntrace++] = (int)ipc; } return 1; }
            if (vm->ntrace) { for (int q = 0; q < vm->ntrace; q++) { vm->lt_m[q] = vm->tr_m[q]; vm->lt_pc[q] = vm->tr_pc[q]; } vm->nlt = vm->ntrace; vm->lt_m[vm->nlt < 12 ? vm->nlt : 11] = vm->lt_m[vm->nlt < 12 ? vm->nlt : 11]; }
            vm->ntrace = 0;
        }
    }
}

/* ---------- threads ---------- */
static uint32_t jnow(VM *vm) { return vm->now ? vm->now() : 0; }
static void thread_main(void) {
    VM *vm = g_vm; JThread *t = vm->cur;
    Method *m = t->startm; int32_t *base = vm->top;
    for (int i = 0; i < t->nsargs; i++) base[i] = t->sargs[i];
    vm->top = base + m->max_locals + m->max_stack + 2; vm->exc = 0;
    int r = 0;
    if (init_class(vm, m->cls)) r = 1; else r = call_method(vm, m, base);
    t->status = r; if (r == 1) t->errname = vm->exc ? obj_class(vm, vm->exc)->name : "(exception)"; else if (r < 0) t->errname = vm->err ? vm->err : "vm error";
    if (r) { vm->err = t->errname; if (vm->on_die) vm->on_die(vm, t, vm->exc); }
    vm->exc = 0; vm->ntrace = 0; t->state = 3;
    for (;;) co_suspend(vm, t);
}
static int spawn_method(VM *vm, Method *m, const int32_t *args, int nargs, uint32_t obj, uint32_t stack_slots) {
    int i; JThread *t = 0;
    for (i = 0; i < VM_MAX_THREADS; i++) if (vm->threads[i].state == 0) { t = &vm->threads[i]; break; }
    if (!t) return -1;
    if (!t->stack || t->ssize < stack_slots) { if (t->stack) free(t->stack); t->stack = (int32_t *)calloc(stack_slots, 4); t->ssize = stack_slots; }
    if (!t->stack) return -1;
    memset(t->stack, 0, t->ssize * 4);
    if (co_create(vm, t)) return -1;
    t->startm = m; t->nsargs = nargs; for (int k = 0; k < nargs; k++) t->sargs[k] = args[k];
    t->top = t->stack; t->depth = 0; t->state = 1; t->wake = 0; t->obj = obj; t->waiton = t->joinon = 0; t->status = 0; t->errname = 0;
    return i;
}
int vm_spawn(VM *vm, uint32_t thread_obj) {
    Method *run = vm_find_method(obj_class(vm, thread_obj), "run", "()V"); if (!run) return -1;
    int32_t a = (int32_t)thread_obj; return spawn_method(vm, run, &a, 1, thread_obj, JSTACK_OTHER);
}
int vm_spawn_sized(VM *vm, uint32_t thread_obj, uint32_t slots) {
    Method *run = vm_find_method(obj_class(vm, thread_obj), "run", "()V"); if (!run) return -1;
    int32_t a = (int32_t)thread_obj; return spawn_method(vm, run, &a, 1, thread_obj, slots);
}
/* call a method synchronously from inside a native (on the current Java thread's stacks); returns 0 ok, 1 exception, <0 VM error */
int vm_call(VM *vm, Method *m, const int32_t *args, int nargs) {
    int32_t *base = vm->top; int need = m->max_locals > nargs ? m->max_locals : nargs;
    if (base + need + m->max_stack + 4 > vm->stack + vm->stack_size) { vm_throw(vm, "java/lang/StackOverflowError"); return 1; }
    for (int i = 0; i < nargs; i++) base[i] = args[i];
    vm->top = base + need + m->max_stack + 2;
    int r = init_class(vm, m->cls) ? 1 : call_method(vm, m, base);
    vm->top = base; return r;
}
int vm_init_class(VM *vm, Class *c) { return init_class(vm, c); }
void vm_yield(VM *vm, uint32_t wake_ms) {
    JThread *t = vm->cur; if (!t) return;
    t->wake = wake_ms; t->state = wake_ms ? 2 : 1; t->top = vm->top; t->depth = vm->depth;
    co_suspend(vm, t);
}
int vm_wait(VM *vm, uint32_t obj, uint32_t wake_ms) {
    JThread *t = vm->cur; if (!t) return 0;
    t->waiton = obj; t->wake = wake_ms; t->state = 2; t->top = vm->top; t->depth = vm->depth;
    co_suspend(vm, t); return 0;
}
void vm_notify(VM *vm, uint32_t obj, int all) {
    for (int i = 0; i < VM_MAX_THREADS; i++) { JThread *t = &vm->threads[i];
        if (t->state == 2 && t->waiton == obj && obj) { t->waiton = 0; t->state = 1; t->wake = 0; if (!all) break; } }
}
int vm_thread_alive(VM *vm, uint32_t obj) {
    for (int i = 0; i < VM_MAX_THREADS; i++) { JThread *t = &vm->threads[i]; if (t->obj == obj && obj && t->state != 0 && t->state != 3) return 1; }
    return 0;
}
static void reap(VM *vm, JThread *t) { co_destroy(t); t->state = 0; t->obj = 0; }
int vm_run(VM *vm, uint32_t budget_ms) {
    uint32_t t0 = jnow(vm); int ran, alive;
    g_vm = vm;
    do {
        ran = 0; alive = 0;
        for (int i = 0; i < VM_MAX_THREADS; i++) {
            JThread *t = &vm->threads[i];
            if (t->state == 3) { reap(vm, t); continue; }
            if (t->state == 0) continue;
            alive++;
            uint32_t now = jnow(vm);
            if (t->state == 2) {
                if (t->wake && (int32_t)(now - t->wake) >= 0) { t->state = 1; t->waiton = 0; }
                else continue;
            }
            vm->cur = t; vm->stack = t->stack; vm->stack_size = t->ssize; vm->top = t->top; vm->depth = t->depth; vm->fuel = FUEL;
            co_resume(vm, t);
            vm->cur = 0; ran++;
        }
    } while (ran && (uint32_t)(jnow(vm) - t0) < budget_ms);
    return alive;
}
int vm_init(VM *vm, uint32_t heap_bytes, uint32_t stack_slots, uint32_t (*now)(void)) {
    memset(vm, 0, sizeof *vm); g_vm = vm; vm->now = now;
    heap_bytes &= ~7u;
    vm->heap = (uint8_t *)malloc(heap_bytes); if (!vm->heap) return -1;
    uint32_t nw = (heap_bytes >> 8) + 1;
    vm->startbits = (uint32_t *)calloc(nw, 4); vm->markbits = (uint32_t *)calloc(nw, 4);
    vm->markcap = 1024; vm->markstack = (uint32_t *)malloc(vm->markcap * 4);
    if (!vm->startbits || !vm->markbits || !vm->markstack) return -1;
    vm->heap_size = heap_bytes; vm->heap_top = 16; vm->used = 16; vm->gc_at = heap_bytes - heap_bytes / 4;
    vm->main_slots = stack_slots; return 0;
}
void vm_set_natives(VM *vm, const NativeEntry *t, int n) { vm->natives = t; vm->nnatives = n; }
/* run a static method to completion on a fresh green thread (drives the scheduler until it ends). For setup code and tests. */
int vm_invoke_static(VM *vm, Method *m, const int32_t *args, int nargs) {
    int idx = spawn_method(vm, m, args, nargs, 0, vm->main_slots); if (idx < 0) return 1;
    JThread *t = &vm->threads[idx];
    while (t->state != 3) vm_run(vm, 20);
    int st = t->status; reap(vm, t); return st ? 1 : 0;
}
