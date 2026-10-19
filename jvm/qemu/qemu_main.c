/* qemu_main.c: runs the J2ME runtime on a bare-metal Cortex-M4 (QEMU mps2-an386) with the exact same freestanding build as the watch app:
 * same sys.c (libc shim + math), same co_switch assembly, libgcc soft double. Output through semihosting. */
#include <stdint.h>
#include <stddef.h>
#include "sys.h"
#include "midp.h"
extern uint8_t jar_data[], jar_end[], rt_data[], rt_end[];
extern uint8_t __arena[];
extern void jm_set_alloc(void *(*m)(uint32_t), void (*f)(void *));

static int sh(int op, void *arg) { register int r0 __asm__("r0") = op; register void *r1 __asm__("r1") = arg; __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory"); return r0; }
static void out(const char *s) { sh(4, (void *)s); }
static void outnum(const char *label, uint32_t v) { char b[40]; int n = 0; uint32_t d[10]; int nd = 0; out(label); if (!v) d[nd++] = 0; while (v) { d[nd++] = v % 10; v /= 10; } while (nd) b[n++] = (char)('0' + d[--nd]); b[n++] = 10; b[n] = 0; out(b); }
static uint8_t *arena_top; static uint8_t *arena_end;
static void *bump(uint32_t n) { n = (n + 7) & ~7u; if (arena_top + n > arena_end) return 0; void *p = arena_top; arena_top += n; return p; }
static void nofree(void *p) { (void)p; }
static uint32_t vt = 1000;
#ifdef USE_SYSTICK
/* SysTick as a clock: with -icount shift=0 virtual time = executed instructions (1 ns each); the mps2 system clock is 25 MHz, so 1 tick = 40 instructions */
static uint32_t last_cvr, ticks_acc, ms_base;
static void st_init(void) { *(volatile uint32_t *)0xE000E014 = 0xFFFFFF; *(volatile uint32_t *)0xE000E018 = 0; *(volatile uint32_t *)0xE000E010 = 5; last_cvr = *(volatile uint32_t *)0xE000E018; }
static uint32_t h_ticks(void) { uint32_t c = *(volatile uint32_t *)0xE000E018; uint32_t d = (last_cvr - c) & 0xFFFFFF; last_cvr = c; static uint32_t frac; frac += d; ms_base += frac / 25000; frac %= 25000; return ms_base + 1000; }
#else
static uint32_t h_ticks(void) { return vt; }
#endif
static void h_log(const char *s) { out("[midlet] "); out(s); out("\n"); }
static uint8_t *h_read(const char *p, uint32_t *l) { (void)p; (void)l; return 0; }
static int h_write(const char *p, const uint8_t *d, uint32_t n) { (void)p; (void)d; (void)n; return 0; }
static int h_del(const char *p) { (void)p; return 0; }
static void h_vib(int ms) { (void)ms; }
static void dump(const uint16_t *s, const char *name) {          /* raw RGB565 frame to a host file via semihosting */
    uint32_t args[3] = { (uint32_t)name, 5 /* wb */, 0 }; while (name[args[2]]) args[2]++;
    int fd = sh(1, args); if (fd < 0) return;
    uint32_t w[3] = { (uint32_t)fd, (uint32_t)s, MIDP_W * MIDP_H_ * 2 }; sh(5, w); uint32_t c[1] = { (uint32_t)fd }; sh(2, c);
}
int main(void) {
#ifdef USE_SYSTICK
    st_init();
#endif
    arena_top = __arena; arena_end = (uint8_t *)0x20400000 - 64 * 1024;
    jm_set_alloc(bump, nofree);
    MidpHost h = { h_read, h_write, h_del, h_log, h_vib, h_ticks };
    uint32_t jl = (uint32_t)(jar_end - jar_data), rl = (uint32_t)(rt_end - rt_data);
    Midp *m = midp_create(&h, jar_data, jl, rt_data, rl, "qemu", 700 * 1024);
    if (!m) { out("midp_create failed\n"); sh(0x18, (void *)0x20023); }
    int r = midp_boot(m); if (r) { outnum("boot failed ", (uint32_t)r); out(midp_error(m) ? midp_error(m) : "?"); out("\n"); sh(0x18, (void *)0x20023); }
    for (int i = 0; i < 6000 && !midp_finished(m); i++) {
        if (i == 120) midp_event(m, 1, 53);
        if (i == 140) midp_event(m, 2, 53);
        midp_run(m, 5);
#ifndef USE_SYSTICK
        vt += 25;
#endif
    }
    int dirty; const uint16_t *scr = midp_screen(m, &dirty);
    uint32_t sum = 0; for (int i = 0; i < MIDP_W * MIDP_H_; i++) sum = sum * 31 + scr[i];
    outnum("screen checksum ", sum); outnum("heap used KB ", midp_heap_used(m) / 1024); outnum("finished ", (uint32_t)midp_finished(m));
    if (midp_error(m)) { out("error: "); out(midp_error(m)); out("\n"); }
    dump(scr, "qemu_frame.raw");
    out("QEMU RUN DONE\n"); sh(0x18, (void *)0x20026);
    for (;;) {}
}
