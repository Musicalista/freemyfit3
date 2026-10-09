/* host test of the Game Boy core: runs a ROM for N frames, optionally presses buttons, writes PPM screenshots. usage: host_gb_test rom.gb frames out.ppm [keys...] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#define C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define MALLOC(n) malloc(n)
#define FREE(p) free(p)
static u16 shot[144][160];
#include "gbcore.inc.c"
static void gb_emit(GB *g, int ly) { memcpy(shot[ly], g->line, 320); }
static void dump(const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n160 144\n255\n");
    for (int y = 0; y < 144; y++) for (int x = 0; x < 160; x++) { unsigned p = shot[y][x]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); u32 n = (u32)ftell(f); fseek(f, 0, SEEK_SET); u8 *rom = malloc(n); fread(rom, 1, n, f); fclose(f);
    GB *g = gb_new(rom, n); if (!g) { puts("gb_new failed"); return 1; }
    printf("cgb=%d mbc=%d banks=%d eram=%u sizeof=%zu\n", g->cgb, g->mbc, g->nbanks, g->ersz, sizeof(GB));
    int frames = atoi(argv[2]); clock_t t0 = clock();
    for (int i = 0; i < frames; i++) {
        /* scripted input: argv[4..] = "frame:buttons:dpad" */
        g->jd = 0; g->jb = 0;
        for (int k = 4; k < argc; k++) { int a, b, c, d; if (sscanf(argv[k], "%d-%d:%d:%d", &a, &b, &c, &d) == 4 && i >= a && i < b) { g->jb |= c; g->jd |= d; } }
        gb_frame(g, 1);
        if (i % 600 == 599) { char nm[200]; snprintf(nm, sizeof nm, "%s.%d.ppm", argv[3], i + 1); dump(nm); }
    }
    double dt = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("%d frames in %.2fs (%.1f fps) pc=%04x ly=%d\n", frames, dt, frames / dt, g->pc, g->ly);
    dump(argv[3]); return 0;
}
