/* PC test for the native rd-132211 port: loads terrain.bmp from the jar, simulates 20ms frames, prints eye path, dumps PPMs. */
#include "rd.h"
#include "jar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void dump_ppm(const char *name, const uint16_t *fb, int w, int h) {
    FILE *f = fopen(name, "wb"); if (!f) return; fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) { uint16_t p = fb[i]; uint8_t c[3] = { (uint8_t)(((p >> 11) & 31) * 255 / 31), (uint8_t)(((p >> 5) & 63) * 255 / 63), (uint8_t)((p & 31) * 255 / 31) }; fwrite(c, 1, 3, f); }
    fclose(f);
}
int main(int argc, char **argv) {
    if (argc < 2) { puts("usage: rd_host game.jar [frames] [dump_every] [w h]"); return 1; }
    FILE *f = fopen(argv[1], "rb"); if (!f) return 1; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *img = malloc(n); if (fread(img, 1, n, f) != (size_t)n) return 1; fclose(f);
    Jar jar; if (jar_open(&jar, img, (uint32_t)n)) return 1;
    uint32_t bl; uint8_t *bmp = jar_read(&jar, "terrain.bmp", &bl); M3dTex tex; if (!bmp || m3d_parse_bmp(bmp, bl, &tex)) { puts("bmp"); return 1; }
    { uint16_t *c = malloc(32 * 16 * 2); for (int y = 0; y < 16; y++) for (int x = 0; x < 32; x++) c[y * 32 + x] = tex.pix[y * tex.w + x]; tex.pix = c; tex.w = 32; tex.h = 16; }
    int frames = argc > 2 ? atoi(argv[2]) : 30, dump = argc > 3 ? atoi(argv[3]) : 0, W = argc > 5 ? atoi(argv[4]) : 240, H = argc > 5 ? atoi(argv[5]) : 320;
    static Rd g; rd_init(&g, 1000000); uint16_t *fb = malloc((size_t)W * H * 2), *zb = malloc((size_t)W * H * 2);
    int64_t now = 1000000; g.last_ms = now; long ticks = 0;
    for (int fr = 1; fr <= frames; fr++) {
        if (fr == 150) { for (int i = 0; i < 20; i++) rd_key(&g, RD_LOOK_DOWN, 1); }
        if (fr == 160 || fr == 165 || fr == 170) { rd_key(&g, RD_BREAK, 1); }
        if (fr == 175) rd_key(&g, RD_PLACE, 1);
        if (fr == 180) { rd_key(&g, RD_FWD, 1); }
        rd_advance(&g, now); for (int i = 0; i < g.ticks; i++) { rd_tick(&g); ticks++; }
        rd_render(&g, g.alpha, &tex, W, H, fb, zb);
        M3dView v; float eye[3]; rd_camera(&g, g.alpha, W, &v, eye);
        if (fr <= 12 || fr == frames) printf("frame %3d ticks %4ld eye=(%.3f %.3f %.3f) ground=%d\n", fr, ticks, eye[0], eye[1], eye[2], g.on_ground);
        if (dump && fr % dump == 0) { char nm[128]; snprintf(nm, sizeof nm, "rd_%04d.ppm", fr); dump_ppm(nm, fb, W, H); }
        now += 20;
    }
    dump_ppm("rd_last.ppm", fb, W, H); return 0;
}
