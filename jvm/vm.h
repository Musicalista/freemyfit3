/* CLDC-style JVM core for the Fit3 watch: interpreter + conservative mark/sweep GC + cooperative threads (one C stack per Java thread).
 * Portable C99; needs sys.h only. References are byte offsets into vm->heap (0 = null). */
#ifndef VM_H
#define VM_H
#include <stdint.h>
#include <stddef.h>

typedef struct VM VM;
typedef struct Class Class;
typedef struct Method Method;
/* returns 0 ok, 1 exception pending (vm->exc set), -2 => StringIndexOutOfBounds, other <0 => VM error; result in vm->ret */
typedef int (*NativeFn)(VM *vm, int32_t *args);

typedef struct { const char *cls, *name, *desc; NativeFn fn; int is_static; } NativeEntry;

#define VM_MAX_THREADS 8
#define VM_MAX_PINS 48
typedef struct JThread JThread;
struct JThread {
    int32_t *stack; uint32_t ssize;           /* Java operand/locals stack (slots) */
    int32_t *top; int depth;                  /* saved while not running */
    uint8_t *cstack; uint32_t cstack_size; void *ctx;   /* C stack + saved context (see co_*) */
    int state;                                /* 0 free, 1 ready, 2 sleeping/waiting, 3 finished */
    uint32_t wake;                            /* wake-up time (ms) when sleeping */
    uint32_t obj;                             /* java.lang.Thread object */
    uint32_t waiton;                          /* object waited on (wait/notify) or 0 */
    uint32_t joinon;                          /* thread object being joined, or 0 */
    Method *startm; int32_t sargs[4]; int nsargs; int status;   /* entry method + args; status 0 ok, 1 uncaught exception, <0 VM error */
    const char *errname;
};

struct VM {
    uint8_t *heap; uint32_t heap_size, heap_top;      /* refs are byte offsets into heap; 0 = null */
    uint32_t used, free_head, gc_at; uint32_t bins[17];                         /* live+garbage bytes allocated, head of the free list (offset, 0 = none) */
    uint32_t *startbits, *markbits;                   /* one bit per 8 heap bytes */
    uint32_t *markstack; uint32_t markcap, marksp; int markover;
    int32_t *stack; uint32_t stack_size;               /* the stack of the running thread (slots) */
    int32_t ret[2];
    uint32_t exc;                                      /* pending exception ref */
    Class **classes; int nclasses, cap_classes;
    const uint8_t *(*load_class)(const char *name, uint32_t *len, void *ud); void *load_ud;
    const NativeEntry *natives; int nnatives;
    int depth; int32_t *top;
    int fuel;                                          /* instruction budget before a preemption check */
    uint32_t (*now)(void);                             /* ms clock */
    JThread threads[VM_MAX_THREADS]; JThread *cur;     /* cur == 0 outside Java threads */
    void *sched_ctx;
    uint32_t *pins[VM_MAX_PINS]; int npins;
    Class *c_string, *c_object;
    uint32_t ngc, main_slots; const char *err; Method *err_m;                     /* gc count, last fatal error text */
    void (*on_die)(VM *vm, JThread *t, uint32_t exc);   /* called when a thread ends with an uncaught exception / VM error */
    Method *tr_m[12]; int tr_pc[12]; int ntrace; Method *lt_m[12]; int lt_pc[12]; int nlt;      /* frames an uncaught exception unwound through (newest first) */
    void *user;
};

int  vm_init(VM *vm, uint32_t heap_bytes, uint32_t stack_slots, uint32_t (*now)(void));
void vm_set_natives(VM *vm, const NativeEntry *t, int n);
Class *vm_find_class(VM *vm, const char *name);
Method *vm_find_method(Class *c, const char *name, const char *desc);
int  vm_field_slot(Class *c, const char *name);    /* instance field slot (int32 index after the 8-byte header), -1 if none */
/* run a method on the CALLING context (not a Java thread); args in 'args' (slots). returns 0 ok, 1 uncaught exception/error */
int  vm_invoke_static(VM *vm, Method *m, const int32_t *args, int nargs);
uint32_t vm_new_string(VM *vm, const char *s);
uint32_t vm_new_array(VM *vm, int type, int32_t len); /* JVM newarray codes, 0 = ref */
uint32_t vm_new_object(VM *vm, Class *c);
const char *vm_class_name(Class *c);
const char *vm_exc_name(VM *vm);
int vm_throw(VM *vm, const char *cls);          /* sets vm->exc; returns 1 */
Class *vm_obj_class(VM *vm, uint32_t ref);
int vm_class_id(Class *c);
const char *vm_method_class(Method *m);
const char *vm_method_name(Method *m);
void vm_pin(VM *vm, uint32_t *slot);            /* the GC treats *slot as a root */
void vm_gc(VM *vm);
/* threads */
int  vm_spawn(VM *vm, uint32_t thread_obj);
int  vm_spawn_sized(VM *vm, uint32_t thread_obj, uint32_t stack_slots);
int  vm_call(VM *vm, Method *m, const int32_t *args, int nargs);   /* synchronous call from a native */
int  vm_init_class(VM *vm, Class *c);     /* starts obj.run() in a new green thread; returns thread index or -1 */
void vm_yield(VM *vm, uint32_t wake_ms);        /* from a native running in a Java thread: sleep until wake_ms (0 = just yield) */
int  vm_wait(VM *vm, uint32_t obj, uint32_t wake_ms);   /* Object.wait */
void vm_notify(VM *vm, uint32_t obj, int all);
int  vm_run(VM *vm, uint32_t budget_ms);        /* scheduler: runs ready threads for about budget_ms; returns number of live threads */
int  vm_thread_alive(VM *vm, uint32_t thread_obj);
int  vm_instance_of(VM *vm, uint32_t ref, const char *cls);
/* heap helpers */
#define VM_REF(vm, r) ((uint8_t *)((vm)->heap + (r)))
#endif
