/* cs.c: original CS-style tactical shooter logic on the voxel world of rd.c (no Valve assets/code).
 * Map = CSLite Boxes.png (cover boxes) in a 64x64 arena. Player vs AI bots, hitscan, reload, respawns. Portable, no libc, no globals. */
#include "cs_map.h"
#define CS_BOTS 5
#define CS_FLOOR 43
#define CS_MAG 30
#define CS_EYE 1.5f
typedef struct { float x, z; int hp, cd, resp, dir, dirt; } CsBot;
typedef struct {
    Rd g; CsBot b[CS_BOTS];
    int hp, ammo, reserve, kills, deaths, fire_cd, reload, flash, hurt, dead, hitmark, fire_req;
    uint32_t rng;
} Cs;
enum { CSE_HURT = 1, CSE_KILL = 2, CSE_DEAD = 4, CSE_SHOT = 8, CSE_HIT = 16 };

static uint32_t cs_rand(Cs *c) { c->rng = c->rng * 1664525u + 1013904223u; return c->rng >> 8; }
static int cs_cell_solid(const Rd *g, float x, float y, float z) { return rd_is_solid(g, (int)rd_floorf(x), (int)rd_floorf(y), (int)rd_floorf(z)); }
static int cs_free(const Rd *g, float x, float z) {
    for (int dy = 0; dy < 2; dy++) {
        float y = (float)CS_FLOOR + 0.1f + (float)dy;
        if (cs_cell_solid(g, x - 0.3f, y, z - 0.3f) || cs_cell_solid(g, x + 0.3f, y, z - 0.3f) || cs_cell_solid(g, x - 0.3f, y, z + 0.3f) || cs_cell_solid(g, x + 0.3f, y, z + 0.3f)) return 0;
    }
    return 1;
}
static int cs_los(const Rd *g, float ax, float ay, float az, float bx, float by, float bz) {
    float dx = bx - ax, dy = by - ay, dz = bz - az, d = rd_sqrtf(dx * dx + dy * dy + dz * dz); int n = (int)(d / 0.25f);
    for (int i = 1; i < n; i++) { float t = (float)i / (float)n; if (cs_cell_solid(g, ax + dx * t, ay + dy * t, az + dz * t)) return 0; }
    return 1;
}
static void cs_spawn_bot(Cs *c, int i) {
    CsBot *b = &c->b[i];
    for (int tries = 0; tries < 60; tries++) {
        float x = 3.0f + (float)(cs_rand(c) % 58u) + 0.5f, z = 3.0f + (float)(cs_rand(c) % 58u) + 0.5f;
        float dx = x - c->g.x, dz = z - c->g.z;
        if (!cs_free(&c->g, x, z) || dx * dx + dz * dz < 18.0f * 18.0f) continue;
        b->x = x; b->z = z; break;
    }
    b->hp = 100; b->cd = 40 + (int)(cs_rand(c) % 60u); b->resp = 0; b->dir = 1; b->dirt = 30 + (int)(cs_rand(c) % 60u);
}
void cs_init(Cs *c, int64_t seed) {
    rd_init(&c->g, seed); c->rng = (uint32_t)seed * 2654435761u + 12345u;
    for (int z = 0; z < RD_H; z++) for (int x = 0; x < RD_W; x++) {
        int ring = x == 0 || z == 0 || x == 63 || z == 63, box = (cs_map_bits[(z * 64 + x) >> 3] >> ((z * 64 + x) & 7)) & 1;
        int h = ring ? 4 : box ? 2 : 0;
        for (int k = 0; k < h; k++) rd_set_solid(&c->g, x, CS_FLOOR + k, z, 1);
    }
    rd_recalc_light(&c->g);
    rd_place_player(&c->g, 10.5f, (float)CS_FLOOR + 0.95f, 12.5f, -45.0f);
    c->hp = 100; c->ammo = CS_MAG; c->reserve = 120; c->kills = 0; c->deaths = 0; c->fire_cd = 0; c->reload = 0; c->flash = 0; c->hurt = 0; c->dead = 0; c->hitmark = 0; c->fire_req = 0;
    for (int i = 0; i < CS_BOTS; i++) cs_spawn_bot(c, i);
}
static void cs_dir(const Cs *c, float *dx, float *dy, float *dz) {
    float yaw = c->g.yrot * RD_DEG, pit = c->g.xrot * RD_DEG, cp = rd_cosf(pit);
    *dx = -rd_sinf(yaw) * cp; *dy = -rd_sinf(pit); *dz = rd_cosf(yaw) * cp;
}
/* ray (o,d) vs bot AABB; returns entry t or -1, and hit height above the bot's feet */
static float cs_ray_bot(const CsBot *b, const float o[3], const float d[3], float *hit_h) {
    float lo[3] = { b->x - 0.3f, (float)CS_FLOOR, b->z - 0.3f }, hi[3] = { b->x + 0.3f, (float)CS_FLOOR + 1.75f, b->z + 0.3f };
    float t0 = 0.0f, t1 = 1e9f;
    for (int a = 0; a < 3; a++) {
        if (d[a] > -1e-6f && d[a] < 1e-6f) { if (o[a] < lo[a] || o[a] > hi[a]) return -1.0f; continue; }
        float ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a]; if (ta > tb) { float t = ta; ta = tb; tb = t; }
        if (ta > t0) t0 = ta; if (tb < t1) t1 = tb; if (t0 > t1) return -1.0f;
    }
    *hit_h = o[1] + d[1] * t0 - (float)CS_FLOOR; return t0;
}
int cs_fire(Cs *c) {
    int ev = 0; float d[3], o[3] = { c->g.x, c->g.y, c->g.z };
    c->ammo--; c->fire_cd = 6; c->flash = 3; ev |= CSE_SHOT; c->g.xrot -= 0.5f;                    /* recoil: look up a little */
    cs_dir(c, &d[0], &d[1], &d[2]);
    float tw = 60.0f;
    for (int i = 2; i < 600; i++) { float t = (float)i * 0.1f; if (cs_cell_solid(&c->g, o[0] + d[0] * t, o[1] + d[1] * t, o[2] + d[2] * t)) { tw = t; break; } }
    int best = -1; float bt = tw, bh = 0.0f;
    for (int i = 0; i < CS_BOTS; i++) {
        if (c->b[i].hp <= 0) continue; float h = 0.0f, t = cs_ray_bot(&c->b[i], o, d, &h);
        if (t >= 0.0f && t < bt) { bt = t; best = i; bh = h; }
    }
    if (best >= 0) {
        CsBot *b = &c->b[best]; int dmg = bh > 1.4f ? 100 : 34; b->hp -= dmg; c->hitmark = 8; ev |= CSE_HIT;
        if (b->hp <= 0) { c->kills++; b->resp = 150 + (int)(cs_rand(c) % 120u); ev |= CSE_KILL; if (c->reserve < 90) c->reserve += 10; }
    }
    return ev;
}
void cs_reload(Cs *c) { if (c->reload == 0 && c->ammo < CS_MAG && c->reserve > 0) c->reload = 90; }

int cs_tick(Cs *c) {
    int ev = 0;
    rd_tick(&c->g);
    if (c->dead) return ev;
    if (c->fire_cd > 0) c->fire_cd--; if (c->flash > 0) c->flash--; if (c->hurt > 0) c->hurt--; if (c->hitmark > 0) c->hitmark--;
    if (c->reload > 0 && --c->reload == 0) { int need = CS_MAG - c->ammo, take = need < c->reserve ? need : c->reserve; c->ammo += take; c->reserve -= take; }
    if (c->fire_req && c->fire_cd == 0 && c->reload == 0) { if (c->ammo > 0) ev |= cs_fire(c); else cs_reload(c); }
    if (c->ammo == 0 && c->reload == 0 && c->reserve > 0) cs_reload(c);
    float px = c->g.x, py = c->g.y, pz = c->g.z;
    for (int i = 0; i < CS_BOTS; i++) {
        CsBot *b = &c->b[i];
        if (b->hp <= 0) { if (--b->resp <= 0) cs_spawn_bot(c, i); continue; }
        float dx = px - b->x, dz = pz - b->z, dist = rd_sqrtf(dx * dx + dz * dz); if (dist < 0.01f) dist = 0.01f;
        float ux = dx / dist, uz = dz / dist, mx, mz;
        if (--b->dirt <= 0) { b->dir = -b->dir; b->dirt = 40 + (int)(cs_rand(c) % 80u); }
        if (dist > 8.0f) { mx = ux; mz = uz; } else if (dist < 4.0f) { mx = -ux; mz = -uz; } else { mx = -uz * (float)b->dir; mz = ux * (float)b->dir; }
        mx *= 0.035f; mz *= 0.035f;
        if (cs_free(&c->g, b->x + mx, b->z + mz)) { b->x += mx; b->z += mz; }
        else if (cs_free(&c->g, b->x + mx, b->z)) b->x += mx; else if (cs_free(&c->g, b->x, b->z + mz)) b->z += mz;
        if (b->cd > 0) { b->cd--; continue; }
        if (dist < 28.0f && cs_los(&c->g, b->x, (float)CS_FLOOR + CS_EYE, b->z, px, py, pz)) {
            b->cd = 40 + (int)(cs_rand(c) % 40u);
            if ((int)(cs_rand(c) % 100u) < 70 - (int)(dist * 1.5f)) {
                int dmg = 5 + (int)(cs_rand(c) % 8u); c->hp -= dmg; c->hurt = 12; ev |= CSE_HURT;
                if (c->hp <= 0) { c->hp = 0; c->dead = 1; c->deaths++; ev |= CSE_DEAD; }
            }
        }
    }
    return ev;
}

/* ---- rendering: world (rd_render) + bots as flat-coloured boxes ---- */
static void cs_box(const M3dTarget *tg, const M3dView *v, float x0, float y0, float z0, float x1, float y1, float z1, uint16_t col) {
    M3dTex t; t.pix = &col; t.w = 1; t.h = 1;
    int32_t X0 = (int32_t)(x0 * 256.0f), X1 = (int32_t)(x1 * 256.0f), Y0 = (int32_t)(y0 * 256.0f), Y1 = (int32_t)(y1 * 256.0f), Z0 = (int32_t)(z0 * 256.0f), Z1 = (int32_t)(z1 * 256.0f);
    static const int32_t uv[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; int32_t c[12], g;
#define Q(ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz, gr) c[0]=ax;c[1]=ay;c[2]=az;c[3]=bx;c[4]=by;c[5]=bz;c[6]=cx;c[7]=cy;c[8]=cz;c[9]=dx;c[10]=dy;c[11]=dz; g=gr; m3d_quads(tg, v, &t, 1, c, uv, &g)
    Q(X0, Y1, Z0, X1, Y1, Z0, X1, Y1, Z1, X0, Y1, Z1, 0xffffff);          /* top */
    Q(X0, Y0, Z0, X1, Y0, Z0, X1, Y1, Z0, X0, Y1, Z0, 0xcccccc);          /* -z */
    Q(X0, Y0, Z1, X1, Y0, Z1, X1, Y1, Z1, X0, Y1, Z1, 0xcccccc);          /* +z */
    Q(X0, Y0, Z0, X0, Y0, Z1, X0, Y1, Z1, X0, Y1, Z0, 0x999999);          /* -x */
    Q(X1, Y0, Z0, X1, Y0, Z1, X1, Y1, Z1, X1, Y1, Z0, 0x999999);          /* +x */
#undef Q
}
void cs_render(const Cs *c, float alpha, const M3dTex *tex, int w, int h, uint16_t *fb, uint16_t *zb) {
    rd_render(&c->g, alpha, tex, w, h, fb, zb);
    M3dView v; float eye[3]; M3dTarget tg; tg.fb = fb; tg.zb = zb; tg.w = w; tg.h = h;
    v.cy = h * 0.5f; rd_camera(&c->g, alpha, w, &v, eye);
    for (int i = 0; i < CS_BOTS; i++) {
        const CsBot *b = &c->b[i]; if (b->hp <= 0) continue;
        float f = (float)CS_FLOOR;
        cs_box(&tg, &v, b->x - 0.22f, f, b->z - 0.18f, b->x + 0.22f, f + 0.8f, b->z + 0.18f, m3d_rgb565(0x3c3c4a));            /* legs */
        cs_box(&tg, &v, b->x - 0.3f, f + 0.8f, b->z - 0.2f, b->x + 0.3f, f + 1.45f, b->z + 0.2f, m3d_rgb565(0xb4402e));        /* torso */
        cs_box(&tg, &v, b->x - 0.17f, f + 1.45f, b->z - 0.17f, b->x + 0.17f, f + 1.78f, b->z + 0.17f, m3d_rgb565(0xe6be96));    /* head */
    }
}
