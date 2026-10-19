/* host_pc.c: runs a MIDlet jar on the PC with the same runtime code as the watch.
 *   host_pc game.jar [rt.pak] [-t seconds] [-o prefix] [-k "ms:type:arg,..."] [-d every_ms] [-h heap_kb]
 * Writes <prefix>_NNN.ppm snapshots. Events: type 1/2 key down/up (arg = key code), 3/4/5 pointer (arg = x + y*65536). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(x) Sleep(x)
static uint32_t now_ms(void) { return (uint32_t)GetTickCount() + 1; }
#else
#include <unistd.h>
#define SLEEP_MS(x) usleep((x) * 1000)
static uint32_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000) + 1; }
#endif
#include "midp.h"

static const char *save_dir = "savedata";
static uint8_t *slurp(const char *p, uint32_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return 0; } fclose(f); *n = (uint32_t)l; return b;
}
static void local_path(const char *path, char *out, size_t cap) { const char *b = strrchr(path, '/'); snprintf(out, cap, "%s/%s", save_dir, b ? b + 1 : path); }
static uint8_t *h_read(const char *path, uint32_t *len) { char p[300]; local_path(path, p, sizeof p); return slurp(p, len); }
static int h_write(const char *path, const uint8_t *d, uint32_t n) { char p[300]; local_path(path, p, sizeof p); FILE *f = fopen(p, "wb"); if (!f) return -1; fwrite(d, 1, n, f); fclose(f); return 0; }
static int h_delete(const char *path) { char p[300]; local_path(path, p, sizeof p); return remove(p); }
static void h_log(const char *s) { printf("[midlet] %s\n", s); fflush(stdout); }
static void h_vib(int ms) { printf("[vibrate %d]\n", ms); }
static void dump(Midp *m, const char *name) {
    int dirty; const uint16_t *s = midp_screen(m, &dirty); FILE *f = fopen(name, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", MIDP_W, MIDP_H_);
    for (int i = 0; i < MIDP_W * MIDP_H_; i++) { uint16_t c = s[i]; uint8_t px[3] = { (uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3) }; fwrite(px, 1, 3, f); }
    fclose(f);
}
int main(int argc, char **argv) {
    const char *jarp = 0, *rtp = "rt.pak", *prefix = "frame", *keys = 0; int secs = 5, every = 0; uint32_t heapkb = 1024;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) secs = atoi(argv[++i]); else if (!strcmp(argv[i], "-o") && i + 1 < argc) prefix = argv[++i];
        else if (!strcmp(argv[i], "-k") && i + 1 < argc) keys = argv[++i]; else if (!strcmp(argv[i], "-d") && i + 1 < argc) every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) heapkb = (uint32_t)atoi(argv[++i]); else if (!jarp) jarp = argv[i]; else rtp = argv[i];
    }
    if (!jarp) { puts("usage: host_pc game.jar [rt.pak] [-t sec] [-o prefix] [-k ms:type:arg,...] [-d every_ms] [-h heap_kb]"); return 1; }
    uint32_t jl, rl; uint8_t *jar = slurp(jarp, &jl), *rt = slurp(rtp, &rl); if (!jar || !rt) { puts("cannot read jar / rt.pak"); return 1; }
    MidpHost h = { h_read, h_write, h_delete, h_log, h_vib, now_ms };
    Midp *m = midp_create(&h, jar, jl, rt, rl, "test", heapkb * 1024); if (!m) { puts("midp_create failed"); return 2; }
    int r = midp_boot(m); if (r) { printf("boot failed (%d): %s\n", r, midp_error(m) ? midp_error(m) : "?"); return 3; }
    uint32_t start = now_ms(), last_dump = start; int frame = 0; const char *kp = keys;
    while ((now_ms() - start) < (uint32_t)secs * 1000 && !midp_finished(m)) {
        uint32_t t = now_ms() - start;
        while (kp && *kp) { int at, ty, arg, n; if (sscanf(kp, "%d:%d:%d%n", &at, &ty, &arg, &n) < 3) break; if ((uint32_t)at > t) break; midp_event(m, ty, arg); kp += n; if (*kp == ',') kp++; }
        midp_run(m, 20);
        if (every && now_ms() - last_dump >= (uint32_t)every) { char nm[300]; snprintf(nm, sizeof nm, "%s_%03d.ppm", prefix, frame++); dump(m, nm); last_dump = now_ms(); }
        SLEEP_MS(5);
    }
    { char nm[300]; snprintf(nm, sizeof nm, "%s_last.ppm", prefix); dump(m, nm); }
    printf("done: finished=%d heap used %u KB, error: %s\n", midp_finished(m), midp_heap_used(m) / 1024, midp_error(m) ? midp_error(m) : "none");
    return 0;
}
