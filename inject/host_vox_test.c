/* host test of the block-world engine: generates each world type, renders a few views to PPM, exercises physics/break/place. usage: host_vox_test out_prefix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vox.c"
static void dump(const uint16_t *fb, int w, int h, const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
int main(int argc, char **argv) {
    Vox *g = malloc(sizeof(Vox)); static uint16_t tex[VX_TEXW * 16], fb[256 * 262], zb[256 * 262]; int W = 256, H = 262;
    vx_make_tex(tex); M3dTex t = { tex, VX_TEXW, 16 }; dump(tex, VX_TEXW, 16, "tex.ppm");
    for (int type = 0; type < VW_TYPES; type++) {
        vx_gen(g, 4242 + (uint32_t)type, type); g->alpha = 1.0f;
        int cnt[VB_N] = {0}; for (int i = 0; i < VX_W * VX_H * VX_D; i++) cnt[g->blk[i]]++;
        printf("type %d: spawn y=%.2f top=%d blocks:", type, g->y, g->top[32 * VX_W + 32]); for (int b = 1; b < VB_N; b++) printf(" %d", cnt[b]); printf("\n");
        g->pitch = 25.0f; g->yaw = 30.0f; vx_tick(g); vx_render(g, &t, W, H, fb, zb);
        char nm[100]; snprintf(nm, sizeof nm, "%s_%d.ppm", argv[1], type); dump(fb, W, H, nm);
    }
    /* physics: walk, fall, jump, break, place */
    vx_gen(g, 7, VW_HILLS); g->flying = 0; g->yaw = 0; for (int i = 0; i < 200; i++) vx_tick(g);
    printf("fell to y=%.3f on_ground=%d (surface %d)\n", g->y - 1.62f, g->on_ground, g->top[32 * VX_W + 32] + 1);
    float z0 = g->z; vx_key(g, VK_FWD, 1); for (int i = 0; i < 120; i++) vx_tick(g); printf("walked dz=%.2f (z %.2f -> %.2f) x=%.2f\n", g->z - z0, z0, g->z, g->x);
    vx_key(g, VK_FWD, 0); g->pitch = 40; for (int i = 0; i < 40; i++) vx_tick(g); printf("has_hit=%d at %d,%d,%d f=%d\n", g->has_hit, g->hit.x, g->hit.y, g->hit.z, g->hit.f);
    int before = vx_get(g, g->hit.x, g->hit.y, g->hit.z), hx = g->hit.x, hy = g->hit.y, hz = g->hit.z; int br = vx_break(g); printf("break: %d block %d -> %d\n", br, before, vx_get(g, hx, hy, hz));
    g->sel = 4; int pl = vx_place(g); printf("place: %d now %d (planks=%d)\n", pl, vx_get(g, hx, hy, hz), VB_PLANKS);
    vx_toggle_fly(g); vx_key(g, VK_UP, 1); float y0 = g->y; for (int i = 0; i < 60; i++) vx_tick(g); printf("fly up: dy=%.2f\n", g->y - y0);
    g->yaw = 200; g->pitch = 20; vx_tick(g); vx_render(g, &t, W, H, fb, zb); dump(fb, W, H, "vox_fly.ppm");
    return 0;
}
