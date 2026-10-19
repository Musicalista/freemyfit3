#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *dir;
static const uint8_t *loader(const char *name, uint32_t *len, void *ud) {
    char path[512]; (void)ud; snprintf(path, sizeof path, "%s/%s.class", dir, name);
    FILE *f = fopen(path, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) { fclose(f); return 0; } fclose(f); *len = (uint32_t)n; return b;
}
static int n_print(VM *vm, int32_t *a) { (void)vm; printf("%d\n", a[0]); return 0; }
static int n_printl(VM *vm, int32_t *a) { (void)vm; printf("%lld\n", (long long)(((uint64_t)(uint32_t)a[1] << 32) | (uint32_t)a[0])); return 0; }
static int n_prints(VM *vm, int32_t *a) {
    uint32_t s = (uint32_t)a[0]; if (!s) { puts("null"); return 0; }
    uint32_t arr = (uint32_t)((int32_t *)(vm->heap + s + 8))[0]; uint32_t n = ((uint32_t *)(vm->heap + arr))[1];
    for (uint32_t i = 0; i < n; i++) putchar(((uint16_t *)(vm->heap + arr + 8))[i]); putchar('\n'); return 0; }
static int n_printf(VM *vm, int32_t *a) { (void)vm; float f; memcpy(&f, a, 4); printf("%.4f\n", f); return 0; }
static int n_printd(VM *vm, int32_t *a) { (void)vm; uint64_t u = ((uint64_t)(uint32_t)a[1] << 32) | (uint32_t)a[0]; double d; memcpy(&d, &u, 8); printf("%.6f\n", d); return 0; }
static const NativeEntry nat[] = { {"Sys", "print", "(I)V", n_print, 1}, {"Sys", "printl", "(J)V", n_printl, 1}, {"Sys", "prints", "(Ljava/lang/String;)V", n_prints, 1}, {"Sys", "printf", "(F)V", n_printf, 1}, {"Sys", "printd", "(D)V", n_printd, 1} };
int main(int argc, char **argv) {
    VM vm; dir = argc > 1 ? argv[1] : "."; const char *cn = argc > 2 ? argv[2] : "T";
    if (vm_init(&vm, 1 << 20, 16384)) return 2;
    vm_set_natives(&vm, nat, 5); vm.load_class = loader;
    Class *c = vm_find_class(&vm, cn); if (!c) { puts("class not found"); return 3; }
    Method *m = vm_find_method(c, "main", "()V"); if (!m) { puts("no main"); return 3; }
    int r = vm_invoke_static(&vm, m, 0, 0);
    if (r) printf("UNCAUGHT: %s\n", vm.exc ? vm_exc_name(&vm) : "(vm error)");
    printf("heap used: %u bytes, classes: %d\n", vm.heap_top, vm.nclasses);
    return r;
}
