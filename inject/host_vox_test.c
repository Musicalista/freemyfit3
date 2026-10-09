/* host test of the block-world engine: generates each world type, renders a few views to PPM, exercises physics/break/place. usage: host_vox_test out_prefix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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

    /* ---- survival + mobs ---- */
    vx_gen(g, 99, VW_HILLS); int pigs = 0; for (int i = 0; i < VX_MOBS; i++) if (g->mobs[i].type == VM_PIG) pigs++; printf("pigs at start: %d\n", pigs);
    vx_set_survival(g, 1); for (int i = 0; i < 300; i++) vx_tick(g);
    printf("survival: health=%d food=%d flying=%d on_ground=%d\n", g->health, g->food, g->flying, g->on_ground);
    /* break grass -> dirt in inventory, stone -> cobble */
    g->pitch = 60; vx_tick(g); int id0 = vx_get(g, g->hit.x, g->hit.y, g->hit.z); vx_break(g); printf("broke id %d: inv dirt=%d cobble=%d\n", id0, g->inv[VB_DIRT], g->inv[VB_COBBLE]);
    g->sel = 1; int p1 = vx_place(g); printf("place dirt: %d inv dirt=%d\n", p1, g->inv[VB_DIRT]); g->sel = 7; printf("place brick without any: %d\n", vx_place(g));
    g->inv[VB_LOG] = 3; printf("craft planks: %d -> planks=%d logs=%d\n", vx_craft(g, 0), g->inv[VB_PLANKS], g->inv[VB_LOG]);
    g->inv[VB_PORK] = 2; g->food = 10; printf("eat: %d food=%d pork=%d\n", vx_eat(g), g->food, g->inv[VB_PORK]);
    /* fall damage: put the player high and let him fall */
    g->y = 35.0f; g->yd = 0; int h0 = g->health; for (int i = 0; i < 120; i++) vx_tick(g); printf("fall: health %d -> %d\n", h0, g->health);
    /* night: jump the clock, wait for a zombie, let it attack, kill it */
    vx_respawn(g); g->time = (uint32_t)(VX_DAY_TICKS * 0.7); int zc = 0; for (int i = 0; i < 1200 && !zc; i++) { vx_tick(g); for (int k = 0; k < VX_MOBS; k++) if (g->mobs[k].type == VM_ZOMBIE) zc++; }
    printf("zombie spawned at night: %d (time %u)\n", zc, g->time);
    VxMob *z = 0; for (int k = 0; k < VX_MOBS; k++) if (g->mobs[k].type == VM_ZOMBIE) z = &g->mobs[k];
    if (z) printf("zombie reached the player: health=%d\n", g->health);
    vx_gen(g, 99, VW_HILLS); vx_set_survival(g, 1); g->flying = 1; g->y += 6.0f; g->yo = g->y; g->pitch = 0; vx_tick(g); g->yd = 0;
    for (int k = 0; k < VX_MOBS; k++) g->mobs[k].type = VM_NONE;
    { g->health = 20; g->hurt = 0; g->yaw = 0; g->pitch = 0; VxMob *a = &g->mobs[0]; a->type = VM_ZOMBIE; a->x = g->x; a->z = g->z + 2.0f; a->y = g->y - 1.62f; a->hp = 20; a->xd = a->yd = a->zd = 0; a->cd = 0; a->fx = 0; a->fz = -1;
      int hits = 0, n = 0; while (a->type && n++ < 8) { hits += vx_attack(g); } printf("attack: hits=%d zombie alive=%d\n", hits, a->type != 0);
      VxMob *b = &g->mobs[1]; b->type = VM_PIG; b->x = g->x; b->z = g->z + 2.0f; b->y = g->y - 1.62f; b->hp = 10; g->pitch = 30; n = 0; hits = 0; while (b->type && n++ < 8) hits += vx_attack(g); printf("pig: hits=%d pork=%d\n", hits, g->inv[VB_PORK]); }
    /* render a night scene with a zombie in front */
    vx_gen(g, 5, VW_DESERT); vx_set_survival(g, 1); g->flying = 1; g->y += 4.0f; g->yo = g->y; vx_tick(g); g->time = (uint32_t)(VX_DAY_TICKS * 0.7); for (int k = 0; k < VX_MOBS; k++) g->mobs[k].type = VM_NONE;
    g->yaw = 0; g->pitch = 40; { VxMob *a = &g->mobs[0]; a->type = VM_ZOMBIE; a->x = g->x + 0.5f; a->z = g->z + 3.0f; a->y = g->y - 1.62f - 4.0f; a->fx = 0; a->fz = -1; a->hp = 20; a->xd = a->yd = a->zd = 0; a->cd = 0;
      VxMob *b = &g->mobs[1]; b->type = VM_PIG; b->x = g->x - 1.2f; b->z = g->z + 2.5f; b->y = g->y - 1.62f - 4.0f; b->fx = 0.7f; b->fz = -0.7f; b->hp = 10; b->xd = b->yd = b->zd = 0; }
    g->alpha = 1; vx_render(g, &t, W, H, fb, zb); dump(fb, W, H, "vox_mobs_night.ppm");
    g->time = 900; g->alpha = 1; vx_render(g, &t, W, H, fb, zb); dump(fb, W, H, "vox_mobs_day.ppm");
    return 0;
}
