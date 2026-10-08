/* Minimal CLDC-style JVM core (int/long/ref, no float). Portable C99; libc: string.h/stdlib.h only. */
#ifndef VM_H
#define VM_H
#include <stdint.h>
#include <stddef.h>

typedef struct VM VM;
typedef struct Class Class;
typedef struct Method Method;
/* returns 0 ok, 1 exception pending (vm->exc set), -2 => StringIndexOutOfBounds; result in vm->ret */
typedef int (*NativeFn)(VM *vm, int32_t *args);

typedef struct { const char *cls, *name, *desc; NativeFn fn; int is_static; } NativeEntry;

struct VM {
    uint8_t *heap; uint32_t heap_size, heap_top;      /* refs are byte offsets into heap; 0 = null */
    int32_t *stack; uint32_t stack_size;               /* in slots */
    int32_t ret[2];
    uint32_t exc;                                      /* pending exception ref */
    Class **classes; int nclasses, cap_classes;
    const uint8_t *(*load_class)(const char *name, uint32_t *len, void *ud); void *load_ud;
    const NativeEntry *natives; int nnatives;
    int depth; int32_t *top;
    void *user;
};

int  vm_init(VM *vm, uint32_t heap_bytes, uint32_t stack_slots);
void vm_set_natives(VM *vm, const NativeEntry *t, int n);
Class *vm_find_class(VM *vm, const char *name);
Method *vm_find_method(Class *c, const char *name, const char *desc);
/* run static method; args in 'args' (slots). returns 0 ok, 1 uncaught exception/error */
int  vm_invoke_static(VM *vm, Method *m, const int32_t *args, int nargs);
uint32_t vm_new_string(VM *vm, const char *s);
uint32_t vm_new_array(VM *vm, int type, int32_t len); /* JVM newarray codes, 0 = ref */
uint32_t vm_new_object(VM *vm, Class *c);
const char *vm_class_name(Class *c);
const char *vm_exc_name(VM *vm);
int vm_throw(VM *vm, const char *cls);          /* sets vm->exc; returns 1 */
Class *vm_obj_class(VM *vm, uint32_t ref);
int vm_class_id(Class *c);
#endif
