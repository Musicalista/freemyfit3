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
void vx_gen(Vox *g, uint32_t seed, int type) {
    uint8_t *b = g->blk; for (int i = 0; i < VX_W * VX_H * VX_D; i++) b[i] = 0;
    g->seed = seed; g->wtype = type; uint32_t s = seed * 2654435761u + 12345u;
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
    g->radius = 11; g->sel = 0; g->flying = 1; g->modified = 0;
    { static const uint8_t hot[9] = { VB_GRASS, VB_DIRT, VB_STONE, VB_COBBLE, VB_PLANKS, VB_LOG, VB_SAND, VB_BRICK, VB_GLASS }; for (int i = 0; i < 9; i++) g->hot[i] = hot[i]; }
    g->x = 32.5f; g->z = 32.5f; g->y = (float)g->top[32 * VX_W + 32] + 1.0f + 1.62f + 0.01f;
    g->xo = g->x; g->yo = g->y; g->zo = g->z; g->xd = g->yd = g->zd = 0; g->yaw = 0; g->pitch = 0; g->on_ground = 0; g->has_hit = 0;
    for (int i = 0; i < VK_N; i++) g->held[i] = 0;
    g->turn_yaw = g->turn_pitch = 0; g->passed = 0; g->ticks = 0;
}

/* ---------------- player ---------------- */
static int box_hits(const Vox *g, float x, float feet, float z) {
    int x0 = (int)rd_floorf(x - 0.29f), x1 = (int)rd_floorf(x + 0.29f), z0 = (int)rd_floorf(z - 0.29f), z1 = (int)rd_floorf(z + 0.29f);
    int y0 = (int)rd_floorf(feet + 0.001f), y1 = (int)rd_floorf(feet + 1.79f);
    for (int yy = y0; yy <= y1; yy++) for (int zz = z0; zz <= z1; zz++) for (int xx = x0; xx <= x1; xx++) if (solid_c(g, xx, yy, zz)) return 1;
    return 0;
}
void vx_toggle_fly(Vox *g) { g->flying ^= 1; g->yd = 0; }
void vx_key(Vox *g, int key, int down) { if (key >= 0 && key < VK_N) g->held[key] = (uint8_t)down; }
void vx_look(Vox *g, float dx, float dy) { g->turn_yaw += dx; g->turn_pitch += dy; }
static void pick(Vox *g, float dist) {
    float yaw = g->yaw * RD_DEG, pit = g->pitch * RD_DEG, cp = rd_cosf(pit);
    float dx = -rd_sinf(yaw) * cp, dy = -rd_sinf(pit), dz = rd_cosf(yaw) * cp;
    int px = (int)rd_floorf(g->x), py = (int)rd_floorf(g->y), pz = (int)rd_floorf(g->z); g->has_hit = 0;
    for (int i = 0; i <= (int)(dist / 0.05f); i++) {
        float t = (float)i * 0.05f; int cx = (int)rd_floorf(g->x + dx * t), cy = (int)rd_floorf(g->y + dy * t), cz = (int)rd_floorf(g->z + dz * t);
        if (solid_v(g, cx, cy, cz)) {
            g->hit.x = cx; g->hit.y = cy; g->hit.z = cz; g->hit.f = py < cy ? 0 : py > cy ? 1 : pz < cz ? 2 : pz > cz ? 3 : px < cx ? 4 : px > cx ? 5 : 1; g->has_hit = 1; return;
        }
        px = cx; py = cy; pz = cz;
    }
}
void vx_tick(Vox *g) {
    if (g->turn_yaw != 0.0f || g->turn_pitch != 0.0f) {
        g->yaw += g->turn_yaw * 0.15f; g->pitch -= g->turn_pitch * 0.15f; g->turn_yaw = g->turn_pitch = 0;
        if (g->pitch < -90.0f) g->pitch = -90.0f; if (g->pitch > 90.0f) g->pitch = 90.0f;
    }
    g->xo = g->x; g->yo = g->y; g->zo = g->z;
    float xa = 0, za = 0;
    if (g->held[VK_FWD]) za -= 1.0f; if (g->held[VK_BACK]) za += 1.0f; if (g->held[VK_LEFT]) xa -= 1.0f; if (g->held[VK_RIGHT]) xa += 1.0f;
    float sp = g->flying ? 0.012f : g->on_ground ? 0.02f : 0.006f, dist = xa * xa + za * za;
    if (dist > 0.01f) { dist = sp / rd_sqrtf(dist); xa *= dist; za *= dist; float sn = rd_sinf(g->yaw * RD_DEG), cs = rd_cosf(g->yaw * RD_DEG); g->xd += xa * cs - za * sn; g->zd += za * cs + xa * sn; }
    if (g->flying) { float t = g->held[VK_UP] ? 0.14f : g->held[VK_DOWN] ? -0.14f : 0.0f; g->yd += (t - g->yd) * 0.35f; }
    else { if (g->held[VK_UP] && g->on_ground) g->yd = 0.12f; g->yd -= 0.005f; }
    /* move in small steps, axis by axis, so nothing tunnels through a wall */
    float fx = g->x, fy = g->y - 1.62f, fz = g->z, mx = g->xd, my = g->yd, mz = g->zd;
    int steps = (int)(((mx < 0 ? -mx : mx) + (my < 0 ? -my : my) + (mz < 0 ? -mz : mz)) / 0.12f) + 1; if (steps > 8) steps = 8;
    int blocked = 0; g->on_ground = 0;
    for (int i = 0; i < steps; i++) {
        float sx = mx / (float)steps, sy = my / (float)steps, sz = mz / (float)steps;
        if (sy != 0.0f) { if (!box_hits(g, fx, fy + sy, fz)) fy += sy; else { if (sy < 0.0f) g->on_ground = 1; g->yd = 0; my = 0; } }
        if (sx != 0.0f) { if (!box_hits(g, fx + sx, fy, fz)) fx += sx; else { g->xd = 0; mx = 0; blocked = 1; } }
        if (sz != 0.0f) { if (!box_hits(g, fx, fy, fz + sz)) fz += sz; else { g->zd = 0; mz = 0; blocked = 1; } }
    }
    if (fy < -4.0f) { fy = (float)g->top[32 * VX_W + 32] + 2.0f; fx = 32.5f; fz = 32.5f; g->yd = 0; }
    if (blocked && g->on_ground && !g->flying && (g->held[VK_FWD] || g->held[VK_BACK] || g->held[VK_LEFT] || g->held[VK_RIGHT])) g->yd = 0.12f;   /* auto-jump up a one-block step */
    g->x = fx; g->y = fy + 1.62f; g->z = fz;
    g->xd *= 0.91f; g->zd *= 0.91f; if (!g->flying) g->yd *= 0.98f; if (g->on_ground) { g->xd *= 0.8f; g->zd *= 0.8f; } if (g->flying) { g->xd *= 0.93f; g->zd *= 0.93f; }
    pick(g, 5.0f);
}
void vx_advance(Vox *g, int64_t now) {
    int32_t d = (int32_t)(now - g->last_ms); g->last_ms = now; if (d < 0) d = 0; if (d > 1000) d = 1000;
    g->passed += (float)d * 60.0f / 1000.0f; g->ticks = (int)g->passed; g->passed -= (float)g->ticks; g->alpha = g->passed;
}
int vx_break(Vox *g) {
    if (!g->has_hit) return 0; int x = g->hit.x, y = g->hit.y, z = g->hit.z;
    if (g->blk[bidx(x, y, z)] == VB_BEDROCK) return 0;
    vx_set(g, x, y, z, 0); g->modified = 1; pick(g, 5.0f); return 1;
}
int vx_place(Vox *g) {
    if (!g->has_hit) return 0; int x = g->hit.x, y = g->hit.y, z = g->hit.z;
    switch (g->hit.f) { case 0: y--; break; case 1: y++; break; case 2: z--; break; case 3: z++; break; case 4: x--; break; default: x++; }
    if (!in_world(x, y, z) || g->blk[bidx(x, y, z)]) return 0;
    float fx = g->x, fy = g->y - 1.62f, fz = g->z;                                  /* not inside the player */
    if ((float)x < fx + 0.3f && (float)(x + 1) > fx - 0.3f && (float)y < fy + 1.8f && (float)(y + 1) > fy && (float)z < fz + 0.3f && (float)(z + 1) > fz - 0.3f) return 0;
    vx_set(g, x, y, z, g->hot[g->sel]); g->modified = 1; pick(g, 5.0f); return 1;
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

typedef struct { const M3dTarget *tg; const M3dView *v; const M3dTex *t; } RCtx;
static void emit(RCtx *r, int tile, float sh, const int x[4], const int y[4], const int z[4]) {
    int32_t c[12], uv[8], col; int u0 = tile * 16, u1 = u0 + 16, s = (int)(sh * 255.0f); if (s > 255) s = 255; if (s < 0) s = 0; col = (s << 16) | (s << 8) | s;
    for (int i = 0; i < 4; i++) { c[i * 3] = x[i] * 256; c[i * 3 + 1] = y[i] * 256; c[i * 3 + 2] = z[i] * 256; }
    uv[0] = u0; uv[1] = 0; uv[2] = u1; uv[3] = 0; uv[4] = u1; uv[5] = 16; uv[6] = u0; uv[7] = 16;
    m3d_quads(r->tg, r->v, r->t, 1, c, uv, &col);
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
void vx_render(const Vox *g, const M3dTex *tex, int w, int h, uint16_t *fb, uint16_t *zb) {
    M3dView v; float eye[3], fw[3]; RCtx ctx; M3dTarget tg; tg.fb = fb; tg.zb = zb; tg.w = w; tg.h = h;
    v.cy = h * 0.5f; camera(g, g->alpha, w, &v, eye, fw); ctx.tg = &tg; ctx.v = &v; ctx.t = tex;
    { uint16_t sky = m3d_rgb565(SKY_RGB), sky2 = m3d_rgb565(0xC8E4FF); for (int y = 0; y < h; y++) { uint16_t c = (y * 100 / h) < 55 ? sky : sky2; for (int x = 0; x < w; x++) fb[y * w + x] = c; } }
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
    if (g->has_hit) {                                                                    /* outline of the targeted block */
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
        default: p = rgb(62, 62, 62, n * 2); break;
        }
        pix[y * VX_TEXW + t * 16 + x] = p;
    }
}
