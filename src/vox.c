#include "vox.h"
#include "rdmath.h"

#define SKY_RGB 0x80B8FFu

/* ---------------- hashing / noise ---------------- */
static uint32_t hsh(int x, int y, uint32_t s) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u + 0x9E3779B9u;
    h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }
static float vnoise(float x, float y, uint32_t s) {                   /* value noise 0..1 */
    int ix = (int)rd_floorf(x), iy = (int)rd_floorf(y); float fx = smooth(x - (float)ix), fy = smooth(y - (float)iy);
    float a = (float)(hsh(ix, iy, s) & 1023) / 1023.0f, b = (float)(hsh(ix + 1, iy, s) & 1023) / 1023.0f;
    float c = (float)(hsh(ix, iy + 1, s) & 1023) / 1023.0f, d = (float)(hsh(ix + 1, iy + 1, s) & 1023) / 1023.0f;
    float top = a + (b - a) * fx, bot = c + (d - c) * fx; return top + (bot - top) * fy;
}
static float fbm(float x, float y, uint32_t s) { return vnoise(x * 0.07f, y * 0.07f, s) * 0.6f + vnoise(x * 0.16f, y * 0.16f, s + 7u) * 0.28f + vnoise(x * 0.38f, y * 0.38f, s + 13u) * 0.12f; }
static uint32_t rnd(Vox *g) { uint32_t x = g->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; g->rng = x; return x; }

/* ---------------- world ---------------- */
static int bidx(int x, int y, int z) { return (y * VX_D + z) * VX_W + x; }
static int in_world(int x, int y, int z) { return x >= 0 && y >= 0 && z >= 0 && x < VX_W && y < VX_H && z < VX_D; }
static void calc_top(Vox *g, int x, int z) { int y = VX_H - 1; while (y > 0 && !g->blk[bidx(x, y, z)]) y--; g->top[z * VX_W + x] = (uint8_t)y; }
void vx_recalc_tops(Vox *g) { for (int z = 0; z < VX_D; z++) for (int x = 0; x < VX_W; x++) calc_top(g, x, z); }
static void vx_set(Vox *g, int x, int y, int z, int id) { if (in_world(x, y, z)) { g->blk[bidx(x, y, z)] = (uint8_t)id; calc_top(g, x, z); } }
static int solid_c(const Vox *g, int x, int y, int z) {                /* collision: the floor and the walls of the world are solid */
    if (y < 0) return 1; if (y >= VX_H) return 0; if (x < 0 || z < 0 || x >= VX_W || z >= VX_D) return 1; return g->blk[bidx(x, y, z)] != 0;
}
static int solid_v(const Vox *g, int x, int y, int z) { return in_world(x, y, z) && g->blk[bidx(x, y, z)] != 0; }

static void tree(Vox *g, int x, int y, int z, uint32_t r) {            /* trunk at (x, y.., z), y = first log */
    int h = 4 + (int)(r & 1);
    for (int i = 0; i < h; i++) if (in_world(x, y + i, z)) g->blk[bidx(x, y + i, z)] = VB_LOG;
    for (int ly = y + h - 2; ly <= y + h + 1; ly++) {
        int rad = ly >= y + h ? 1 : 2;
        for (int dz = -rad; dz <= rad; dz++) for (int dx = -rad; dx <= rad; dx++) {
            if (rad == 2 && dx * dx == 4 && dz * dz == 4 && ((r >> (dx + dz + 6)) & 1)) continue;          /* rounded corners */
            if (in_world(x + dx, ly, z + dz) && !g->blk[bidx(x + dx, ly, z + dz)]) g->blk[bidx(x + dx, ly, z + dz)] = VB_LEAVES;
        }
    }
}
static void spawn_mob(Vox *g, int type, float x, float z) {
    int i; for (i = 0; i < VX_MOBS && g->mobs[i].type; i++) {} if (i == VX_MOBS) return;
    VxMob *m = &g->mobs[i]; int cx = (int)rd_floorf(x), cz = (int)rd_floorf(z); if (cx < 1 || cz < 1 || cx >= VX_W - 1 || cz >= VX_D - 1) return;
    m->type = type; m->x = x; m->z = z; m->y = (float)g->top[cz * VX_W + cx] + 1.01f; m->xd = m->yd = m->zd = 0; m->hp = type == VM_ZOMBIE ? 20 : 10; m->cd = 0; m->t = 0; m->on_ground = 0; m->fx = 0; m->fz = 1;
}
void vx_gen(Vox *g, uint32_t seed, int type) {
    uint8_t *b = g->blk; for (int i = 0; i < VX_W * VX_H * VX_D; i++) b[i] = 0;
    g->seed = seed; g->wtype = type; uint32_t s = seed * 2654435761u + 12345u; g->rng = s | 1u;
    for (int z = 0; z < VX_D; z++) for (int x = 0; x < VX_W; x++) {
        float n = fbm((float)x, (float)z, s); int h;
        switch (type) {
        case VW_FLAT: h = 14; break;
        case VW_MOUNTAINS: { float m = n * n * 1.8f; h = 10 + (int)(m * 26.0f); } break;
        case VW_DESERT: h = 12 + (int)(n * 7.0f); break;
        default: h = 10 + (int)(n * 14.0f); break;
        }
        if (h > VX_H - 6) h = VX_H - 6; if (h < 3) h = 3;
        for (int y = 0; y <= h; y++) {
            int id;
            if (y == 0) id = VB_BEDROCK;
            else if (y < h - 3) id = ((hsh(x * 7 + y, z * 13 + y * 5, s) % 100) < 2 && y < h - 5) ? VB_COAL : VB_STONE;
            else if (y < h) id = (type == VW_DESERT || (type != VW_FLAT && h <= 12)) ? VB_SAND : (type == VW_MOUNTAINS && h > 24 ? VB_STONE : VB_DIRT);
            else {
                if (type == VW_DESERT || (type != VW_FLAT && h <= 12)) id = VB_SAND;
                else if (type == VW_MOUNTAINS && h > 30) id = VB_SNOW;
                else if (type == VW_MOUNTAINS && h > 25) id = VB_STONE;
                else id = VB_GRASS;
            }
            b[bidx(x, y, z)] = (uint8_t)id;
        }
    }
    if (type == VW_HILLS || type == VW_FLAT) {
        for (int z = 3; z < VX_D - 3; z++) for (int x = 3; x < VX_W - 3; x++) {
            uint32_t r = hsh(x, z, s ^ 0xABCDEFu); int y = VX_H - 1; while (y > 0 && !b[bidx(x, y, z)]) y--;
            if (b[bidx(x, y, z)] == VB_GRASS && (r % 100) < (type == VW_FLAT ? 1u : 2u) && y + 8 < VX_H) {
                int near = 0; for (int dz = -3; dz <= 3 && !near; dz++) for (int dx = -3; dx <= 3; dx++) if (b[bidx(x + dx, y + 1, z + dz)] == VB_LOG) { near = 1; break; }
                if (!near) tree(g, x, y + 1, z, r >> 8);
            }
        }
    }
    vx_recalc_tops(g);
    g->radius = 11; g->sel = 0; g->flying = 1; g->modified = 0; g->survival = 0; g->dead = 0; g->hurt = 0; g->time = 900; g->health = 20; g->food = 20; g->hungert = g->regent = g->spawnt = 0;
    for (int i = 0; i < VB_N + 1; i++) g->inv[i] = 0;
    for (int i = 0; i < VX_MOBS; i++) g->mobs[i].type = VM_NONE;
    { static const uint8_t hot[9] = { VB_GRASS, VB_DIRT, VB_STONE, VB_COBBLE, VB_PLANKS, VB_LOG, VB_SAND, VB_BRICK, VB_GLASS }; for (int i = 0; i < 9; i++) g->hot[i] = hot[i]; }
    g->x = 32.5f; g->z = 32.5f; g->y = (float)g->top[32 * VX_W + 32] + 1.0f + 1.62f + 0.01f;
    g->xo = g->x; g->yo = g->y; g->zo = g->z; g->xd = g->yd = g->zd = 0; g->yaw = 0; g->pitch = 0; g->on_ground = 0; g->has_hit = 0;
    for (int i = 0; i < VK_N; i++) g->held[i] = 0;
    g->turn_yaw = g->turn_pitch = 0; g->passed = 0; g->ticks = 0;
    if (type != VW_DESERT) for (int k = 0, n = 0; k < 80 && n < 6; k++) {                      /* a few pigs on the grass */
        int x = 4 + (int)(rnd(g) % 56u), z = 4 + (int)(rnd(g) % 56u); int y = g->top[z * VX_W + x];
        if (b[bidx(x, y, z)] == VB_GRASS && !b[bidx(x, y + 1, z)]) { spawn_mob(g, VM_PIG, (float)x + 0.5f, (float)z + 0.5f); n++; }
    }
}
void vx_set_survival(Vox *g, int on) {
    g->survival = on; g->health = 20; g->food = 20; g->hurt = 0; g->dead = 0; g->hungert = g->regent = g->spawnt = 0; g->time = 900;
    for (int i = 0; i < VB_N + 1; i++) g->inv[i] = 0;
    if (on) g->flying = 0; else g->flying = 1;
    for (int i = 0; i < VX_MOBS; i++) if (g->mobs[i].type == VM_ZOMBIE) g->mobs[i].type = VM_NONE;
}
void vx_respawn(Vox *g) {
    g->x = 32.5f; g->z = 32.5f; g->y = (float)g->top[32 * VX_W + 32] + 1.0f + 1.62f + 0.01f; g->xo = g->x; g->yo = g->y; g->zo = g->z; g->xd = g->yd = g->zd = 0;
    g->health = 20; g->food = 20; g->hurt = 0; g->dead = 0; g->hungert = g->regent = 0;
    for (int i = 0; i < VX_MOBS; i++) if (g->mobs[i].type == VM_ZOMBIE) g->mobs[i].type = VM_NONE;
}

/* ---------------- physics ---------------- */
static int box_hits(const Vox *g, float x, float feet, float z, float hw, float h) {
    int x0 = (int)rd_floorf(x - hw), x1 = (int)rd_floorf(x + hw), z0 = (int)rd_floorf(z - hw), z1 = (int)rd_floorf(z + hw);
    int y0 = (int)rd_floorf(feet + 0.001f), y1 = (int)rd_floorf(feet + h - 0.01f);
    for (int yy = y0; yy <= y1; yy++) for (int zz = z0; zz <= z1; zz++) for (int xx = x0; xx <= x1; xx++) if (solid_c(g, xx, yy, zz)) return 1;
    return 0;
}
/* moves a body (feet position) by its velocity in small steps, axis by axis; returns 1 if a wall stopped it horizontally */
static int move_body(const Vox *g, float *px, float *pf, float *pz, float *vx, float *vy, float *vz, float hw, float h, int *ground) {
    float fx = *px, fy = *pf, fz = *pz, mx = *vx, my = *vy, mz = *vz;
    int steps = (int)(((mx < 0 ? -mx : mx) + (my < 0 ? -my : my) + (mz < 0 ? -mz : mz)) / 0.12f) + 1, blocked = 0; if (steps > 8) steps = 8;
    *ground = 0;
    for (int i = 0; i < steps; i++) {
        float sx = mx / (float)steps, sy = my / (float)steps, sz = mz / (float)steps;
        if (sy != 0.0f) { if (!box_hits(g, fx, fy + sy, fz, hw, h)) fy += sy; else { if (sy < 0.0f) *ground = 1; *vy = 0; my = 0; } }
        if (sx != 0.0f) { if (!box_hits(g, fx + sx, fy, fz, hw, h)) fx += sx; else { *vx = 0; mx = 0; blocked = 1; } }
        if (sz != 0.0f) { if (!box_hits(g, fx, fy, fz + sz, hw, h)) fz += sz; else { *vz = 0; mz = 0; blocked = 1; } }
    }
    *px = fx; *pf = fy; *pz = fz; return blocked;
}
void vx_toggle_fly(Vox *g) { if (g->survival) return; g->flying ^= 1; g->yd = 0; }
void vx_key(Vox *g, int key, int down) { if (key >= 0 && key < VK_N) g->held[key] = (uint8_t)down; }
void vx_look(Vox *g, float dx, float dy) { g->turn_yaw += dx; g->turn_pitch += dy; }
static void pick(Vox *g, float dist) {
    float yaw = g->yaw * RD_DEG, pit = g->pitch * RD_DEG, cp = rd_cosf(pit);
    float dx = -rd_sinf(yaw) * cp, dy = -rd_sinf(pit), dz = rd_cosf(yaw) * cp;
    int px = (int)rd_floorf(g->x), py = (int)rd_floorf(g->y), pz = (int)rd_floorf(g->z); g->has_hit = 0; g->hit_t = 99.0f;
    for (int i = 0; i <= (int)(dist / 0.05f); i++) {
        float t = (float)i * 0.05f; int cx = (int)rd_floorf(g->x + dx * t), cy = (int)rd_floorf(g->y + dy * t), cz = (int)rd_floorf(g->z + dz * t);
        if (solid_v(g, cx, cy, cz)) {
            g->hit.x = cx; g->hit.y = cy; g->hit.z = cz; g->hit.f = py < cy ? 0 : py > cy ? 1 : pz < cz ? 2 : pz > cz ? 3 : px < cx ? 4 : px > cx ? 5 : 1; g->has_hit = 1; g->hit_t = t; return;
        }
        px = cx; py = cy; pz = cz;
    }
}
static float daylight(const Vox *g) {
    if (!g->survival) return 1.0f;
    float p = (float)(g->time % VX_DAY_TICKS) / (float)VX_DAY_TICKS;
    if (p < 0.5f) return 1.0f; if (p < 0.58f) return 1.0f - (p - 0.5f) / 0.08f; if (p < 0.88f) return 0.0f; if (p < 0.96f) return (p - 0.88f) / 0.08f; return 1.0f;
}
static void hurt_player(Vox *g, int dmg, float dx, float dz) {
    if (g->hurt > 0 || g->dead) return;
    g->health -= dmg; g->hurt = 24; g->xd += dx * 0.12f; g->zd += dz * 0.12f; if (g->yd < 0.06f) g->yd = 0.06f;
    if (g->health <= 0) { g->health = 0; g->dead = 1; }
}
static void mob_tick(Vox *g, VxMob *m) {
    float px = g->x, pz = g->z, dx = px - m->x, dz = pz - m->z, d = rd_sqrtf(dx * dx + dz * dz) + 0.001f, sp = 0.0f, hw = m->type == VM_ZOMBIE ? 0.3f : 0.35f, h = m->type == VM_ZOMBIE ? 1.85f : 0.85f;
    if (m->type == VM_ZOMBIE) {
        if (d < 24.0f && !g->dead) { m->fx = dx / d; m->fz = dz / d; sp = 0.04f; }
        if (m->cd > 0) m->cd--;
        float pf = g->y - 1.62f;
        if (d < 1.0f && m->y < pf + 1.7f && m->y + 1.8f > pf && m->cd == 0 && !g->dead) { hurt_player(g, 3, m->fx, m->fz); m->cd = 50; }
        if (g->time % 20u == 0u && daylight(g) > 0.9f && (rnd(g) & 15u) == 0u) m->type = VM_NONE;                         /* burns away in daylight */
        if (d > 48.0f) m->type = VM_NONE;
    } else {
        if (--m->t <= 0) { m->t = 80 + (int)(rnd(g) & 127u); if (rnd(g) & 1u) { float a = (float)(rnd(g) & 255u) * (2.0f * RD_PI / 256.0f); m->fx = rd_sinf(a); m->fz = rd_cosf(a); m->cd = 1; } else m->cd = 0; }
        if (m->cd) sp = 0.018f;
    }
    if (m->on_ground) { m->xd = m->fx * sp; m->zd = m->fz * sp; }
    m->yd -= 0.005f; if (m->yd < -0.5f) m->yd = -0.5f;
    float feet = m->y, vx = m->xd, vy = m->yd, vz = m->zd; int gr;
    int blocked = move_body(g, &m->x, &feet, &m->z, &vx, &vy, &vz, hw, h, &gr);
    m->y = feet; m->xd = vx; m->yd = vy; m->zd = vz; m->on_ground = gr;
    if (blocked && gr && sp > 0.0f) m->yd = 0.12f;                                                                   /* hop up a step */
    if (m->y < -4.0f) m->type = VM_NONE;
}
void vx_tick(Vox *g) {
    if (g->turn_yaw != 0.0f || g->turn_pitch != 0.0f) {
        g->yaw += g->turn_yaw * 0.15f; g->pitch -= g->turn_pitch * 0.15f; g->turn_yaw = g->turn_pitch = 0;
        if (g->pitch < -90.0f) g->pitch = -90.0f; if (g->pitch > 90.0f) g->pitch = 90.0f;
    }
    g->xo = g->x; g->yo = g->y; g->zo = g->z;
    float xa = 0, za = 0; int dead = g->dead;
    if (!dead) { if (g->held[VK_FWD]) za -= 1.0f; if (g->held[VK_BACK]) za += 1.0f; if (g->held[VK_LEFT]) xa -= 1.0f; if (g->held[VK_RIGHT]) xa += 1.0f; }
    float sp = g->flying ? 0.012f : g->on_ground ? 0.02f : 0.006f, dist = xa * xa + za * za;
    if (dist > 0.01f) { dist = sp / rd_sqrtf(dist); xa *= dist; za *= dist; float sn = rd_sinf(g->yaw * RD_DEG), cs = rd_cosf(g->yaw * RD_DEG); g->xd += xa * cs - za * sn; g->zd += za * cs + xa * sn; }
    if (g->flying) { float t = g->held[VK_UP] && !dead ? 0.14f : g->held[VK_DOWN] && !dead ? -0.14f : 0.0f; g->yd += (t - g->yd) * 0.35f; }
    else { if (g->held[VK_UP] && g->on_ground && !dead) g->yd = 0.12f; g->yd -= 0.005f; }
    float fx = g->x, fy = g->y - 1.62f, fz = g->z, vy0 = g->yd; int was = g->on_ground, gr;
    int blocked = move_body(g, &fx, &fy, &fz, &g->xd, &g->yd, &g->zd, 0.29f, 1.8f, &gr); g->on_ground = gr;
    if (fy < -4.0f) { fy = (float)g->top[32 * VX_W + 32] + 2.0f; fx = 32.5f; fz = 32.5f; g->yd = 0; }
    if (blocked && gr && !g->flying && (g->held[VK_FWD] || g->held[VK_BACK] || g->held[VK_LEFT] || g->held[VK_RIGHT])) g->yd = 0.12f;   /* auto-jump up a one-block step */
    if (g->survival && gr && !was && vy0 < -0.14f) { int dmg = (int)((-vy0 - 0.14f) * 100.0f); if (dmg > 0) { int h0 = g->hurt; g->hurt = 0; hurt_player(g, dmg, 0, 0); if (!g->dead && h0 > g->hurt) g->hurt = h0; } }
    g->x = fx; g->y = fy + 1.62f; g->z = fz;
    g->xd *= 0.91f; g->zd *= 0.91f; if (!g->flying) g->yd *= 0.98f; if (g->on_ground) { g->xd *= 0.8f; g->zd *= 0.8f; } if (g->flying) { g->xd *= 0.93f; g->zd *= 0.93f; }
    pick(g, 5.0f);
    if (g->hurt > 0) g->hurt--;
    if (!g->survival) { for (int i = 0; i < VX_MOBS; i++) if (g->mobs[i].type == VM_PIG) mob_tick(g, &g->mobs[i]); return; }
    /* ---- survival: clock, hunger, regeneration, mobs ---- */
    g->time++;
    if (!g->dead) {
        if (++g->hungert >= 2700) { g->hungert = 0; if (g->food > 0) g->food--; }
        if (g->food >= 14 && g->health < 20) { if (++g->regent >= 300) { g->regent = 0; g->health++; } }
        else if (g->food == 0) { if (++g->regent >= 240) { g->regent = 0; if (g->health > 1) g->health--; } }
        else g->regent = 0;
    }
    if (++g->spawnt >= 90) {
        g->spawnt = 0; int zc = 0, pc = 0; for (int i = 0; i < VX_MOBS; i++) { if (g->mobs[i].type == VM_ZOMBIE) zc++; else if (g->mobs[i].type == VM_PIG) pc++; }
        float a = (float)(rnd(g) & 255u) * (2.0f * RD_PI / 256.0f), r = 12.0f + (float)(rnd(g) & 7u), sx = g->x + rd_sinf(a) * r, sz = g->z + rd_cosf(a) * r;
        if (daylight(g) < 0.4f && zc < 4) spawn_mob(g, VM_ZOMBIE, sx, sz);
        else if (pc < 3 && g->wtype != VW_DESERT) { int cx = (int)rd_floorf(sx), cz = (int)rd_floorf(sz); if (cx > 0 && cz > 0 && cx < VX_W - 1 && cz < VX_D - 1 && g->blk[bidx(cx, g->top[cz * VX_W + cx], cz)] == VB_GRASS) spawn_mob(g, VM_PIG, sx, sz); }
    }
    for (int i = 0; i < VX_MOBS; i++) if (g->mobs[i].type) mob_tick(g, &g->mobs[i]);
}
void vx_advance(Vox *g, int64_t now) {
    int32_t d = (int32_t)(now - g->last_ms); g->last_ms = now; if (d < 0) d = 0; if (d > 1000) d = 1000;
    g->passed += (float)d * 60.0f / 1000.0f; g->ticks = (int)g->passed; g->passed -= (float)g->ticks; g->alpha = g->passed;
}

/* ---------------- actions ---------------- */
int vx_break(Vox *g) {
    if (!g->has_hit || g->dead) return 0; int x = g->hit.x, y = g->hit.y, z = g->hit.z, id = g->blk[bidx(x, y, z)];
    if (id == VB_BEDROCK) return 0;
    if (g->survival) {
        int drop = id == VB_GRASS ? VB_DIRT : id == VB_STONE ? VB_COBBLE : id == VB_GLASS ? 0 : id == VB_LEAVES ? ((rnd(g) % 3u) == 0u ? VB_LEAVES : 0) : id;
        if (drop && g->inv[drop] < 250) g->inv[drop]++;
    }
    vx_set(g, x, y, z, 0); g->modified = 1; pick(g, 5.0f); return 1;
}
int vx_place(Vox *g) {
    if (!g->has_hit || g->dead) return 0; int x = g->hit.x, y = g->hit.y, z = g->hit.z, id = g->hot[g->sel];
    switch (g->hit.f) { case 0: y--; break; case 1: y++; break; case 2: z--; break; case 3: z++; break; case 4: x--; break; default: x++; }
    if (!in_world(x, y, z) || g->blk[bidx(x, y, z)]) return 0;
    if (g->survival && !g->inv[id]) return 0;
    float fx = g->x, fy = g->y - 1.62f, fz = g->z;                                  /* not inside the player */
    if ((float)x < fx + 0.3f && (float)(x + 1) > fx - 0.3f && (float)y < fy + 1.8f && (float)(y + 1) > fy && (float)z < fz + 0.3f && (float)(z + 1) > fz - 0.3f) return 0;
    if (g->survival) g->inv[id]--;
    vx_set(g, x, y, z, id); g->modified = 1; pick(g, 5.0f); return 1;
}
int vx_attack(Vox *g) {
    if (g->dead) return 0;
    float yaw = g->yaw * RD_DEG, pit = g->pitch * RD_DEG, cp = rd_cosf(pit), dx = -rd_sinf(yaw) * cp, dy = -rd_sinf(pit), dz = rd_cosf(yaw) * cp, best = 3.6f; int bi = -1;
    if (g->has_hit && g->hit_t < best) best = g->hit_t;
    for (int i = 0; i < VX_MOBS; i++) {
        VxMob *m = &g->mobs[i]; if (!m->type) continue;
        float hw = m->type == VM_ZOMBIE ? 0.3f : 0.4f, hh = m->type == VM_ZOMBIE ? 1.9f : 0.9f;
        float lo[3] = { m->x - hw, m->y, m->z - hw }, hi[3] = { m->x + hw, m->y + hh, m->z + hw }, o[3] = { g->x, g->y, g->z }, d[3] = { dx, dy, dz };
        float t0 = 0.0f, t1 = best; int ok = 1;
        for (int a = 0; a < 3 && ok; a++) {
            if (d[a] > -1e-6f && d[a] < 1e-6f) { if (o[a] < lo[a] || o[a] > hi[a]) ok = 0; continue; }
            float ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a]; if (ta > tb) { float s = ta; ta = tb; tb = s; }
            if (ta > t0) t0 = ta; if (tb < t1) t1 = tb; if (t0 > t1) ok = 0;
        }
        if (ok && t0 < best) { best = t0; bi = i; }
    }
    if (bi < 0) return 0;
    VxMob *m = &g->mobs[bi]; float hx = -rd_sinf(yaw), hz = rd_cosf(yaw);
    m->hp -= 5; m->xd += hx * 0.2f; m->zd += hz * 0.2f; m->yd = 0.1f; m->cd = m->type == VM_ZOMBIE ? 20 : m->cd;
    if (m->type == VM_PIG) { m->fx = hx; m->fz = hz; m->cd = 1; m->t = 100; }
    if (m->hp <= 0) { if (m->type == VM_PIG && g->survival && g->inv[VB_PORK] < 250) g->inv[VB_PORK] += (uint8_t)(1 + (rnd(g) & 1u)); m->type = VM_NONE; }
    return 1;
}
int vx_eat(Vox *g) {
    if (!g->survival || g->dead || !g->inv[VB_PORK] || g->food >= 20) return 0;
    g->inv[VB_PORK]--; g->food += 6; if (g->food > 20) g->food = 20; return 1;
}
int vx_craft(Vox *g, int r) {
    static const uint8_t from[3] = { VB_LOG, VB_SAND, VB_COBBLE }, nf[3] = { 1, 2, 4 }, to[3] = { VB_PLANKS, VB_GLASS, VB_BRICK }, nt[3] = { 4, 2, 2 };
    if (r < 0 || r > 2 || g->inv[from[r]] < nf[r] || g->inv[to[r]] + nt[r] > 250) return 0;
    g->inv[from[r]] -= nf[r]; g->inv[to[r]] += nt[r]; return 1;
}

/* ---------------- camera + mesh ---------------- */
static void camera(const Vox *g, float a, int w, M3dView *v, float eye[3], float fwd[3]) {
    float px = g->xo + (g->x - g->xo) * a, py = g->yo + (g->y - g->yo) * a, pz = g->zo + (g->z - g->zo) * a;
    float yaw = g->yaw * RD_DEG, pit = g->pitch * RD_DEG, cp = rd_cosf(pit);
    float zx = -rd_sinf(yaw) * cp, zy = -rd_sinf(pit), zz = rd_cosf(yaw) * cp;
    float d = zy, ux = -d * zx, uy = 1.0f - d * zy, uz = -d * zz, l = rd_sqrtf(ux * ux + uy * uy + uz * uz);
    ux /= l; uy /= l; uz /= l; float yx = -ux, yy = -uy, yz = -uz;
    float xx = yy * zz - yz * zy, xy = yz * zx - yx * zz, xz = yx * zy - yy * zx;
    float ex = px * 256.0f, ey = py * 256.0f, ez = pz * 256.0f;
    v->m[0] = xx; v->m[1] = xy; v->m[2] = xz; v->m[3] = -(xx * ex + xy * ey + xz * ez);
    v->m[4] = yx; v->m[5] = yy; v->m[6] = yz; v->m[7] = -(yx * ex + yy * ey + yz * ez);
    v->m[8] = zx; v->m[9] = zy; v->m[10] = zz; v->m[11] = -(zx * ex + zy * ey + zz * ez);
    v->cx = w * 0.5f; v->f = (w * 0.5f) / rd_tanf(800.0f * RD_PI / 4096.0f); v->znear = 100.0f; v->zfar = 32767.0f;
    eye[0] = px; eye[1] = py; eye[2] = pz; fwd[0] = zx; fwd[1] = zy; fwd[2] = zz;
}
static const uint8_t face_tiles[VB_N][3] = {
    {0,0,0}, {0,1,2}, {2,2,2}, {3,3,3}, {4,4,4}, {5,5,5}, {6,7,6}, {8,8,8}, {9,9,9}, {10,10,10}, {11,11,11}, {12,12,12}, {13,13,13}, {14,14,14}, {15,15,15}, {16,16,16} };
int vx_face_tile(int id, int face) { return face_tiles[id < VB_N ? id : 0][face]; }

typedef struct { const M3dTarget *tg; const M3dView *v; const M3dTex *t; float nf; } RCtx;
static void emit(RCtx *r, int tile, float sh, const int x[4], const int y[4], const int z[4]) {
    int32_t c[12], uv[8], col; int u0 = tile * 16, u1 = u0 + 16, s = (int)(sh * r->nf * 255.0f); if (s > 255) s = 255; if (s < 0) s = 0; col = (s << 16) | (s << 8) | s;
    for (int i = 0; i < 4; i++) { c[i * 3] = x[i] * 256; c[i * 3 + 1] = y[i] * 256; c[i * 3 + 2] = z[i] * 256; }
    uv[0] = u0; uv[1] = 0; uv[2] = u1; uv[3] = 0; uv[4] = u1; uv[5] = 16; uv[6] = u0; uv[7] = 16;
    m3d_quads(r->tg, r->v, r->t, 1, c, uv, &col);
}
/* a box of a mob: anchor (ax,ay,az) on the floor, forward (fx,fz), local centre (lx,ly,lz) and half sizes; the "front" (+z) face may use another tile */
static void mob_box(RCtx *r, const float eye[3], float ax, float ay, float az, float fx, float fz, float lx, float ly, float lz, float hx, float hy, float hz, int tile, int ftile) {
    static const float fshade[6] = { 0.7f, 0.7f, 1.0f, 0.55f, 0.85f, 0.85f };            /* -x +x +y -y -z +z */
    for (int f = 0; f < 6; f++) {
        int a = f >> 1; float s = (f & 1) ? 1.0f : -1.0f, q[4][3]; int ft = (f == 5) ? ftile : tile;
        for (int k = 0; k < 4; k++) {
            float u, vv, w; static const float cx4[4] = { -1, 1, 1, -1 }, cy4[4] = { 1, 1, -1, -1 };
            if (a == 1) { u = cx4[k] * hx; vv = (k < 2 ? -hz : hz); w = s * hy; q[k][0] = u; q[k][1] = w; q[k][2] = vv; }
            else if (a == 0) { u = cx4[k] * hz; vv = cy4[k] * hy; w = s * hx; q[k][0] = w; q[k][1] = vv; q[k][2] = u; }
            else { u = cx4[k] * hx; vv = cy4[k] * hy; w = s * hz; q[k][0] = u; q[k][1] = vv; q[k][2] = w; }
        }
        float nx = a == 0 ? s : 0.0f, ny = a == 1 ? s : 0.0f, nz = a == 2 ? s : 0.0f;
        float wnx = nx * fz + nz * fx, wnz = -nx * fx + nz * fz;                           /* rotated normal */
        float cxw = ax + (lx * fz + lz * fx), cyw = ay + ly, czw = az + (-lx * fx + lz * fz);
        float fcx = cxw + wnx * (a == 0 ? hx : a == 2 ? hz : 0.0f), fcy = cyw + ny * hy, fcz = czw + wnz * (a == 0 ? hx : a == 2 ? hz : 0.0f);
        if (wnx * (eye[0] - fcx) + ny * (eye[1] - fcy) + wnz * (eye[2] - fcz) <= 0.0f) continue;   /* face turned away */
        int X[4], Y[4], Z[4];
        for (int k = 0; k < 4; k++) {
            float ox = lx + q[k][0], oz = lz + q[k][2];
            X[k] = (int)((ax + ox * fz + oz * fx) * 256.0f); Y[k] = (int)((ay + ly + q[k][1]) * 256.0f); Z[k] = (int)((az - ox * fx + oz * fz) * 256.0f);
        }
        int32_t c[12], uv[8], col; int u0 = ft * 16, u1 = u0 + 16, sv = (int)(fshade[f] * r->nf * 255.0f); if (sv > 255) sv = 255; col = (sv << 16) | (sv << 8) | sv;
        for (int k = 0; k < 4; k++) { c[k * 3] = X[k]; c[k * 3 + 1] = Y[k]; c[k * 3 + 2] = Z[k]; }
        uv[0] = u0; uv[1] = 0; uv[2] = u1; uv[3] = 0; uv[4] = u1; uv[5] = 16; uv[6] = u0; uv[7] = 16;
        m3d_quads(r->tg, r->v, r->t, 1, c, uv, &col);
    }
}
static void draw_mob(RCtx *r, const float eye[3], const VxMob *m) {
    if (m->type == VM_ZOMBIE) {
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, -0.125f, 0.35f, 0, 0.125f, 0.35f, 0.125f, 19, 19);
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, 0.125f, 0.35f, 0, 0.125f, 0.35f, 0.125f, 19, 19);
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, 0, 1.05f, 0, 0.25f, 0.35f, 0.15f, 18, 18);
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, 0, 1.65f, 0, 0.25f, 0.25f, 0.25f, 17, 20);
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, -0.35f, 1.3f, 0.3f, 0.1f, 0.1f, 0.35f, 17, 17);
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, 0.35f, 1.3f, 0.3f, 0.1f, 0.1f, 0.35f, 17, 17);
    } else {
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, 0, 0.55f, 0, 0.3f, 0.25f, 0.45f, 21, 21);
        mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, 0, 0.65f, 0.6f, 0.2f, 0.2f, 0.2f, 21, 22);
        for (int k = 0; k < 4; k++) mob_box(r, eye, m->x, m->y, m->z, m->fx, m->fz, (k & 1) ? 0.18f : -0.18f, 0.15f, (k & 2) ? 0.3f : -0.3f, 0.07f, 0.15f, 0.07f, 21, 21);
    }
}
static float cell_bright(const Vox *g, int x, int y, int z) { if (!in_world(x, y, z)) return 1.0f; return y < (int)g->top[z * VX_W + x] ? 0.78f : 1.0f; }
static void line(uint16_t *fb, int w, int h, int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, n = 0;
    for (;;) {
        if (x0 >= 0 && y0 >= 0 && x0 < w && y0 < h) fb[y0 * w + x0] = c;
        if ((x0 == x1 && y0 == y1) || ++n > 400) break;
        int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static uint16_t lerp565(uint32_t day, uint32_t night, float t) {
    int r = (int)((float)((day >> 16) & 255) * t + (float)((night >> 16) & 255) * (1.0f - t)), gg = (int)((float)((day >> 8) & 255) * t + (float)((night >> 8) & 255) * (1.0f - t)), b = (int)((float)(day & 255) * t + (float)(night & 255) * (1.0f - t));
    return m3d_rgb565(((uint32_t)r << 16) | ((uint32_t)gg << 8) | (uint32_t)b);
}
void vx_render(const Vox *g, const M3dTex *tex, int w, int h, uint16_t *fb, uint16_t *zb) {
    M3dView v; float eye[3], fw[3]; RCtx ctx; M3dTarget tg; tg.fb = fb; tg.zb = zb; tg.w = w; tg.h = h;
    v.cy = h * 0.5f; camera(g, g->alpha, w, &v, eye, fw); ctx.tg = &tg; ctx.v = &v; ctx.t = tex;
    float dl = daylight(g); ctx.nf = 0.28f + 0.72f * dl;
    { uint16_t sky = lerp565(SKY_RGB, 0x070B22u, dl), sky2 = lerp565(0xC8E4FFu, 0x141C3Cu, dl); for (int y = 0; y < h; y++) { uint16_t c = (y * 100 / h) < 55 ? sky : sky2; for (int x = 0; x < w; x++) fb[y * w + x] = c; } }
    m3d_clear_depth(&tg);
    int cx = (int)rd_floorf(eye[0]), cy = (int)rd_floorf(eye[1]), cz = (int)rd_floorf(eye[2]), r = g->radius;
    int x0 = cx - r, x1 = cx + r, y0 = cy - r, y1 = cy + r, z0 = cz - r, z1 = cz + r;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (z0 < 0) z0 = 0; if (x1 >= VX_W) x1 = VX_W - 1; if (y1 >= VX_H) y1 = VX_H - 1; if (z1 >= VX_D) z1 = VX_D - 1;
    for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
        int id = g->blk[bidx(x, y, z)]; if (!id) continue;
        float dx = (float)x + 0.5f - eye[0], dy = (float)y + 0.5f - eye[1], dz = (float)z + 0.5f - eye[2];
        if (dx * fw[0] + dy * fw[1] + dz * fw[2] < -1.8f) continue;                       /* behind the camera */
        int X[4], Y[4], Z[4];
        if (eye[1] > (float)(y + 1) && !solid_v(g, x, y + 1, z)) { X[0]=x;Y[0]=y+1;Z[0]=z; X[1]=x+1;Y[1]=y+1;Z[1]=z; X[2]=x+1;Y[2]=y+1;Z[2]=z+1; X[3]=x;Y[3]=y+1;Z[3]=z+1; emit(&ctx, face_tiles[id][0], cell_bright(g, x, y + 1, z), X, Y, Z); }
        if (eye[1] < (float)y && !solid_v(g, x, y - 1, z)) { X[0]=x;Y[0]=y;Z[0]=z+1; X[1]=x+1;Y[1]=y;Z[1]=z+1; X[2]=x+1;Y[2]=y;Z[2]=z; X[3]=x;Y[3]=y;Z[3]=z; emit(&ctx, face_tiles[id][2], cell_bright(g, x, y - 1, z) * 0.55f, X, Y, Z); }
        if (eye[2] < (float)z && !solid_v(g, x, y, z - 1)) { X[0]=x;Y[0]=y+1;Z[0]=z; X[1]=x+1;Y[1]=y+1;Z[1]=z; X[2]=x+1;Y[2]=y;Z[2]=z; X[3]=x;Y[3]=y;Z[3]=z; emit(&ctx, face_tiles[id][1], cell_bright(g, x, y, z - 1) * 0.8f, X, Y, Z); }
        if (eye[2] > (float)(z + 1) && !solid_v(g, x, y, z + 1)) { X[0]=x+1;Y[0]=y+1;Z[0]=z+1; X[1]=x;Y[1]=y+1;Z[1]=z+1; X[2]=x;Y[2]=y;Z[2]=z+1; X[3]=x+1;Y[3]=y;Z[3]=z+1; emit(&ctx, face_tiles[id][1], cell_bright(g, x, y, z + 1) * 0.8f, X, Y, Z); }
        if (eye[0] < (float)x && !solid_v(g, x - 1, y, z)) { X[0]=x;Y[0]=y+1;Z[0]=z+1; X[1]=x;Y[1]=y+1;Z[1]=z; X[2]=x;Y[2]=y;Z[2]=z; X[3]=x;Y[3]=y;Z[3]=z+1; emit(&ctx, face_tiles[id][1], cell_bright(g, x - 1, y, z) * 0.65f, X, Y, Z); }
        if (eye[0] > (float)(x + 1) && !solid_v(g, x + 1, y, z)) { X[0]=x+1;Y[0]=y+1;Z[0]=z; X[1]=x+1;Y[1]=y+1;Z[1]=z+1; X[2]=x+1;Y[2]=y;Z[2]=z+1; X[3]=x+1;Y[3]=y;Z[3]=z; emit(&ctx, face_tiles[id][1], cell_bright(g, x + 1, y, z) * 0.65f, X, Y, Z); }
    }
    for (int i = 0; i < VX_MOBS; i++) {
        const VxMob *m = &g->mobs[i]; if (!m->type) continue;
        float dx = m->x - eye[0], dy = m->y + 0.9f - eye[1], dz = m->z - eye[2];
        if (dx * dx + dz * dz > 20.0f * 20.0f || dx * fw[0] + dy * fw[1] + dz * fw[2] < -1.0f) continue;
        draw_mob(&ctx, eye, m);
    }
    if (g->has_hit && (g->hit_t < 90.0f)) {                                                /* outline of the targeted block */
        int sxp[8], syp[8], ok[8];
        for (int i = 0; i < 8; i++) {
            float X = (float)(g->hit.x + (i & 1)) * 256.0f, Y = (float)(g->hit.y + ((i >> 1) & 1)) * 256.0f, Z = (float)(g->hit.z + ((i >> 2) & 1)) * 256.0f;
            float vx = v.m[0] * X + v.m[1] * Y + v.m[2] * Z + v.m[3], vy = v.m[4] * X + v.m[5] * Y + v.m[6] * Z + v.m[7], vz = v.m[8] * X + v.m[9] * Y + v.m[10] * Z + v.m[11];
            ok[i] = vz > v.znear; if (ok[i]) { sxp[i] = (int)(v.cx + v.f * vx / vz); syp[i] = (int)(v.cy + v.f * vy / vz); }
        }
        static const uint8_t ed[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
        for (int e = 0; e < 12; e++) if (ok[ed[e][0]] && ok[ed[e][1]]) line(fb, w, h, sxp[ed[e][0]], syp[ed[e][0]], sxp[ed[e][1]], syp[ed[e][1]], 0x0000);
    }
    for (int i = -3; i <= 3; i++) { int x = w / 2 + i, y = h / 2; if (x >= 0 && x < w) fb[y * w + x] = 0xffff; x = w / 2; y = h / 2 + i; if (y >= 0 && y < h) fb[y * w + x] = 0xffff; }
}

/* ---------------- procedural textures ---------------- */
static int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static uint16_t rgb(int r, int g, int b, int j) { return m3d_rgb565(((uint32_t)clamp8(r + j) << 16) | ((uint32_t)clamp8(g + j) << 8) | (uint32_t)clamp8(b + j)); }
void vx_make_tex(uint16_t *pix) {
    for (int t = 0; t < VX_TILES; t++) for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        uint32_t hh = hsh(x, y, (uint32_t)t * 31u + 5u); int n = (int)(hh & 31) - 16, c; uint16_t p;
        switch (t) {
        case 0: p = rgb(84, 158, 52, n); if ((hh >> 8) % 11 == 0) p = rgb(56, 120, 38, 0); break;
        case 1: if (y < 3 || (y == 3 && (hh & 1))) p = rgb(84, 158, 52, n); else p = rgb(134, 96, 67, n); break;
        case 2: p = rgb(134, 96, 67, n); if ((hh >> 8) % 13 == 0) p = rgb(104, 72, 48, 0); break;
        case 3: p = rgb(126, 126, 126, n * 3 / 2); if ((hh >> 8) % 9 == 0) p = rgb(96, 96, 96, 0); break;
        case 4: p = rgb(120, 120, 120, n); if (y % 5 == 0 || ((x + (y / 5) * 3) % 6) == 0) p = rgb(66, 66, 70, n / 2); break;
        case 5: p = rgb(220, 206, 150, n / 2); break;
        case 6: { int dx = x * 2 - 15, dy = y * 2 - 15, d = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy); c = d / 3; p = d >= 14 ? rgb(88, 68, 40, n / 2) : (c & 1) ? rgb(176, 138, 84, n / 2) : rgb(146, 108, 62, n / 2); } break;
        case 7: p = rgb(106, 82, 50, n / 2); if ((x & 3) == 0) p = rgb(82, 62, 36, n / 2); break;
        case 8: p = rgb(180, 144, 92, n / 2); if ((y & 3) == 3) p = rgb(120, 92, 54, 0); else if (((x + (y >> 2) * 5) & 7) == 0) p = rgb(150, 116, 70, 0); break;
        case 9: p = rgb(44, 122, 36, n * 3 / 2); if ((hh >> 8) % 5 == 0) p = rgb(26, 84, 24, 0); break;
        case 10: p = rgb(152, 72, 56, n); if ((y & 3) == 3 || (((x + ((y >> 2) & 1) * 4) & 7) == 0)) p = rgb(196, 188, 176, 0); break;
        case 11: p = rgb(196, 228, 242, n / 3); if (x == 0 || y == 0 || x == 15 || y == 15) p = rgb(240, 250, 255, 0); else if (x == y || x == y + 1) p = rgb(236, 248, 255, 0); break;
        case 12: p = rgb(182, 44, 44, n / 2); break;
        case 13: p = rgb(50, 84, 192, n / 2); break;
        case 14: p = rgb(242, 246, 250, n / 3); break;
        case 15: p = rgb(126, 126, 126, n * 3 / 2); { int bx = (x % 8), by = (y % 8); uint32_t bh = hsh(x / 8, y / 8, 99u); int cx = 2 + (int)(bh & 3), cy = 2 + (int)((bh >> 4) & 3); if ((bx - cx) * (bx - cx) + (by - cy) * (by - cy) <= 2) p = rgb(30, 30, 34, n / 2); } break;
        case 17: p = rgb(78, 142, 66, n / 2); break;                                                                   /* zombie skin */
        case 18: p = rgb(40, 112, 156, n / 2); break;                                                                  /* zombie shirt */
        case 19: p = rgb(52, 52, 124, n / 2); break;                                                                   /* zombie pants */
        case 20: p = rgb(78, 142, 66, n / 2); if ((y == 5 || y == 6) && ((x >= 3 && x <= 5) || (x >= 10 && x <= 12))) p = rgb(20, 20, 20, 0); else if (y == 11 && x >= 5 && x <= 10) p = rgb(40, 60, 40, 0); break;   /* zombie face */
        case 21: p = rgb(236, 152, 150, n / 2); break;                                                                 /* pig */
        case 22: p = rgb(236, 152, 150, n / 2); if (x >= 4 && x <= 11 && y >= 8 && y <= 12) { p = rgb(246, 186, 176, 0); if ((x == 6 || x == 9) && y == 10) p = rgb(120, 60, 60, 0); } if (y == 5 && (x == 3 || x == 12)) p = rgb(20, 20, 20, 0); break;   /* pig face */
        default: p = rgb(62, 62, 62, n * 2); break;
        }
        pix[y * VX_TEXW + t * 16 + x] = p;
    }
}
