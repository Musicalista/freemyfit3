#include "rd.h"
#include "rdmath.h"
static void rd_zero(void *p, unsigned n) { unsigned char *c = (unsigned char *)p; while (n--) *c++ = 0; }

#define SKY 8433919u

/* ---------------- java.util.Random (LCG) ---------------- */
static int rnd_next(Rd *g, int bits) {
    g->seed48 = (g->seed48 * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
    return (int)(int32_t)(uint32_t)(g->seed48 >> (48 - bits));
}
static float rnd_float(Rd *g) { return (float)rnd_next(g, 24) / (float)(1 << 24); }

/* ---------------- level ---------------- */
static int bidx(int x, int y, int z) { return (y * RD_H + z) * RD_W + x; }
static int in_range(int x, int y, int z) { return x >= 0 && y >= 0 && z >= 0 && x < RD_W && y < RD_D && z < RD_H; }
int rd_is_solid(const Rd *g, int x, int y, int z) {
    if (!in_range(x, y, z)) return 0;
    int i = bidx(x, y, z); return (g->bits[i >> 3] >> (i & 7)) & 1;
}
static void set_bit(Rd *g, int x, int y, int z, int v) {
    int i = bidx(x, y, z); if (v) g->bits[i >> 3] |= (uint8_t)(1 << (i & 7)); else g->bits[i >> 3] &= (uint8_t)~(1 << (i & 7));
}
static void calc_light(Rd *g, int x, int z) {
    int y = RD_D - 1; while (y > 0 && !rd_is_solid(g, x, y, z)) y--;
    g->light[x + z * RD_W] = (uint8_t)y;
}
static float brightness(const Rd *g, int x, int y, int z) {
    if (!in_range(x, y, z)) return 1.0f;
    return y < g->light[x + z * RD_W] ? 0.8f : 1.0f;
}
static void set_tile(Rd *g, int x, int y, int z, int v) {
    if (!in_range(x, y, z) || rd_is_solid(g, x, y, z) == v) return;
    set_bit(g, x, y, z, v); calc_light(g, x, z);
}


static void set_pos(Rd *g, float x, float y, float z);
/* --- helpers for other games built on the same voxel world (e.g. cs.c) --- */
void rd_set_solid(Rd *g, int x, int y, int z, int v) { if (in_range(x, y, z)) set_bit(g, x, y, z, v); }
void rd_recalc_light(Rd *g) { for (int z = 0; z < RD_H; z++) for (int x = 0; x < RD_W; x++) calc_light(g, x, z); }
void rd_place_player(Rd *g, float x, float y, float z, float yaw) {
    set_pos(g, x, y, z); g->xo = g->x; g->yo = g->y; g->zo = g->z; g->yrot = yaw; g->xrot = 0.0f; g->xd = g->yd = g->zd = 0.0f; g->on_ground = 0;
}

/* ---------------- AABB ---------------- */
static RdBox bx(float a, float b, float c, float d, float e, float f) { RdBox r = { a, b, c, d, e, f }; return r; }
static RdBox bb_expand(RdBox b, float xa, float ya, float za) {
    if (xa < 0) b.x0 += xa; if (xa > 0) b.x1 += xa; if (ya < 0) b.y0 += ya; if (ya > 0) b.y1 += ya; if (za < 0) b.z0 += za; if (za > 0) b.z1 += za; return b;
}
static float clip_x(const RdBox *s, const RdBox *c, float xa) {
    if (c->y1 <= s->y0 || c->y0 >= s->y1) return xa; if (c->z1 <= s->z0 || c->z0 >= s->z1) return xa;
    float m; if (xa > 0 && c->x0 >= s->x1) { m = c->x0 - s->x1; if (m < xa) xa = m; }
    if (xa < 0 && c->x1 <= s->x0) { m = c->x1 - s->x0; if (m > xa) xa = m; } return xa;
}
static float clip_y(const RdBox *s, const RdBox *c, float ya) {
    if (c->x1 <= s->x0 || c->x0 >= s->x1) return ya; if (c->z1 <= s->z0 || c->z0 >= s->z1) return ya;
    float m; if (ya > 0 && c->y0 >= s->y1) { m = c->y0 - s->y1; if (m < ya) ya = m; }
    if (ya < 0 && c->y1 <= s->y0) { m = c->y1 - s->y0; if (m > ya) ya = m; } return ya;
}
static float clip_z(const RdBox *s, const RdBox *c, float za) {
    if (c->x1 <= s->x0 || c->x0 >= s->x1) return za; if (c->y1 <= s->y0 || c->y0 >= s->y1) return za;
    float m; if (za > 0 && c->z0 >= s->z1) { m = c->z0 - s->z1; if (m < za) za = m; }
    if (za < 0 && c->z1 <= s->z0) { m = c->z1 - s->z0; if (m > za) za = m; } return za;
}
#define MAXC 32
static int get_cubes(const Rd *g, const RdBox *a, RdBox *out) {
    int x0 = (int)a->x0, x1 = (int)(a->x1 + 1.0f), y0 = (int)a->y0, y1 = (int)(a->y1 + 1.0f), z0 = (int)a->z0, z1 = (int)(a->z1 + 1.0f), n = 0;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (z0 < 0) z0 = 0; if (x1 > RD_W) x1 = RD_W; if (y1 > RD_D) y1 = RD_D; if (z1 > RD_H) z1 = RD_H;
    for (int x = x0; x < x1; x++) for (int y = y0; y < y1; y++) for (int z = z0; z < z1; z++)
        if (rd_is_solid(g, x, y, z) && n < MAXC) out[n++] = bx((float)x, (float)y, (float)z, (float)(x + 1), (float)(y + 1), (float)(z + 1));
    return n;
}

/* ---------------- player ---------------- */
static void set_pos(Rd *g, float x, float y, float z) {
    g->x = x; g->y = y; g->z = z; float w = 0.3f, h = 0.9f; g->bb = bx(x - w, y - h, z - w, x + w, y + h, z + w);
}
static void reset_pos(Rd *g) {
    float x = rnd_float(g) * (float)RD_W, z = rnd_float(g) * (float)RD_H, y = (float)RD_D * 2.0f / 3.0f + 4.0f;
    set_pos(g, x, y, z);
}
void rd_init(Rd *g, int64_t seed) {
    rd_zero(g, sizeof *g);
    g->seed48 = ((uint64_t)seed ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1);
    for (int y = 0; y < RD_D; y++) for (int z = 0; z < RD_H; z++) for (int x = 0; x < RD_W; x++) set_bit(g, x, y, z, y <= RD_D * 2 / 3);
    for (int z = 0; z < RD_H; z++) for (int x = 0; x < RD_W; x++) calc_light(g, x, z);
    g->radius = 12; reset_pos(g); g->xo = g->x; g->yo = g->y; g->zo = g->z;
}
static void turn(Rd *g, float dx, float dy) {
    g->yrot += dx * 0.15f; g->xrot -= dy * 0.15f;
    if (g->xrot < -90.0f) g->xrot = -90.0f; if (g->xrot > 90.0f) g->xrot = 90.0f;
}
static void move(Rd *g, float xa, float ya, float za) {
    float xo = xa, yo = ya, zo = za; RdBox cubes[MAXC]; RdBox e = bb_expand(g->bb, xa, ya, za); int n = get_cubes(g, &e, cubes), i;
    for (i = 0; i < n; i++) ya = clip_y(&g->bb, &cubes[i], ya);
    g->bb.y0 += ya; g->bb.y1 += ya;
    for (i = 0; i < n; i++) xa = clip_x(&g->bb, &cubes[i], xa);
    g->bb.x0 += xa; g->bb.x1 += xa;
    for (i = 0; i < n; i++) za = clip_z(&g->bb, &cubes[i], za);
    g->bb.z0 += za; g->bb.z1 += za;
    g->on_ground = (yo != ya && yo < 0.0f);
    if (xo != xa) g->xd = 0.0f; if (yo != ya) g->yd = 0.0f; if (zo != za) g->zd = 0.0f;
    g->x = (g->bb.x0 + g->bb.x1) / 2.0f; g->y = g->bb.y0 + 1.62f; g->z = (g->bb.z0 + g->bb.z1) / 2.0f;
}
static void move_relative(Rd *g, float xa, float za, float speed) {
    float dist = xa * xa + za * za; if (dist < 0.01f) return;
    dist = speed / rd_sqrtf(dist); xa *= dist; za *= dist;
    float sn = rd_sinf(g->yrot * RD_DEG), cs = rd_cosf(g->yrot * RD_DEG);
    g->xd += xa * cs - za * sn; g->zd += za * cs + xa * sn;
}
static void pick(Rd *g, float dist) {
    float yaw = g->yrot * RD_DEG, pit = g->xrot * RD_DEG, cp = rd_cosf(pit);
    float dx = -rd_sinf(yaw) * cp, dy = -rd_sinf(pit), dz = rd_cosf(yaw) * cp;
    float ex = g->x, ey = g->y, ez = g->z; int steps = (int)(dist / 0.05f);
    int px = (int)rd_floorf(ex), py = (int)rd_floorf(ey), pz = (int)rd_floorf(ez);
    g->has_hit = 0;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i * 0.05f; int cx = (int)rd_floorf(ex + dx * t), cy = (int)rd_floorf(ey + dy * t), cz = (int)rd_floorf(ez + dz * t);
        if (rd_is_solid(g, cx, cy, cz)) {
            int f = py < cy ? 0 : py > cy ? 1 : pz < cz ? 2 : pz > cz ? 3 : px < cx ? 4 : px > cx ? 5 : 1;
            g->hit.x = cx; g->hit.y = cy; g->hit.z = cz; g->hit.f = f; g->has_hit = 1; return;
        }
        px = cx; py = cy; pz = cz;
    }
}
void rd_tick(Rd *g) {
    if (g->turn_yaw != 0.0f || g->turn_pitch != 0.0f) { turn(g, g->turn_yaw, g->turn_pitch); g->turn_yaw = 0; g->turn_pitch = 0; }
    g->xo = g->x; g->yo = g->y; g->zo = g->z;
    float xa = 0, ya = 0;
    if (g->held[RD_RESET]) reset_pos(g);
    if (g->held[RD_FWD]) ya -= 1.0f; if (g->held[RD_BACK]) ya += 1.0f; if (g->held[RD_LEFT]) xa -= 1.0f; if (g->held[RD_RIGHT]) xa += 1.0f;
    if (g->held[RD_JUMP] && g->on_ground) g->yd = 0.12f;
    move_relative(g, xa, ya, g->on_ground ? 0.02f : 0.005f);
    g->yd -= 0.005f;
    move(g, g->xd, g->yd, g->zd);
    g->xd *= 0.91f; g->yd *= 0.98f; g->zd *= 0.91f;
    if (g->on_ground) { g->xd *= 0.8f; g->zd *= 0.8f; }
    pick(g, 4.0f);
}
void rd_look(Rd *g, float dx, float dy) { g->turn_yaw += dx; g->turn_pitch += dy; }
void rd_key(Rd *g, int key, int down) {
    if (key < RD_N_HELD) { g->held[key] = (uint8_t)down; return; }
    if (!down) return;
    switch (key) {
    case RD_TURN_L: g->turn_yaw -= 12.0f; break; case RD_TURN_R: g->turn_yaw += 12.0f; break;
    case RD_LOOK_UP: g->turn_pitch += 12.0f; break; case RD_LOOK_DOWN: g->turn_pitch -= 12.0f; break;
    case RD_BREAK: if (g->has_hit) set_tile(g, g->hit.x, g->hit.y, g->hit.z, 0); break;
    case RD_PLACE: if (g->has_hit) { int x = g->hit.x, y = g->hit.y, z = g->hit.z;
        switch (g->hit.f) { case 0: y--; break; case 1: y++; break; case 2: z--; break; case 3: z++; break; case 4: x--; break; default: x++; }
        set_tile(g, x, y, z, 1); } break;
    }
}
void rd_advance(Rd *g, int64_t now) {
    int32_t d = (int32_t)(now - g->last_ms); g->last_ms = now; if (d < 0) d = 0; if (d > 1000) d = 1000;
    g->passed += (float)d * 1.0f * 60.0f / 1000.0f; g->ticks = (int)g->passed; g->passed -= (float)g->ticks; g->alpha = g->passed;
}

/* ---------------- camera & mesh ---------------- */
void rd_camera(const Rd *g, float a, int w, M3dView *v, float *eye) {
    float px = g->xo + (g->x - g->xo) * a, py = g->yo + (g->y - g->yo) * a, pz = g->zo + (g->z - g->zo) * a;
    float yaw = g->yrot * RD_DEG, pit = g->xrot * RD_DEG, cp = rd_cosf(pit);
    float zx = -rd_sinf(yaw) * cp, zy = -rd_sinf(pit), zz = rd_cosf(yaw) * cp;
    /* up = (0,1,0) orthogonalised; y_down = -up; x = y_down x z */
    float d = zy, ux = -d * zx, uy = 1.0f - d * zy, uz = -d * zz, l = rd_sqrtf(ux * ux + uy * uy + uz * uz);
    ux /= l; uy /= l; uz /= l; float yx = -ux, yy = -uy, yz = -uz;
    float xx = yy * zz - yz * zy, xy = yz * zx - yx * zz, xz = yx * zy - yy * zx;
    float ex = px * 256.0f, ey = py * 256.0f, ez = pz * 256.0f;
    v->m[0] = xx; v->m[1] = xy; v->m[2] = xz; v->m[3] = -(xx * ex + xy * ey + xz * ez);
    v->m[4] = yx; v->m[5] = yy; v->m[6] = yz; v->m[7] = -(yx * ex + yy * ey + yz * ez);
    v->m[8] = zx; v->m[9] = zy; v->m[10] = zz; v->m[11] = -(zx * ex + zy * ey + zz * ez);
    v->cx = w * 0.5f; v->f = (w * 0.5f) / rd_tanf(800.0f * RD_PI / 4096.0f); v->znear = 100.0f; v->zfar = 32767.0f;
    eye[0] = px; eye[1] = py; eye[2] = pz;
}
static uint32_t shade(float b) { int v = (int)(b * 255.0f); if (v < 0) v = 0; if (v > 255) v = 255; return ((uint32_t)v << 16) | ((uint32_t)v << 8) | (uint32_t)v; }
void rd_gen(const Rd *g, const float eye[3], RdQuadFn fn, void *ud) {
    int cx = (int)g->x, cy = (int)g->y, cz = (int)g->z, r = g->radius; int32_t c[12];
#define Q(X0,Y0,Z0,X1,Y1,Z1,X2,Y2,Z2,X3,Y3,Z3) do { c[0]=(X0)*256;c[1]=(Y0)*256;c[2]=(Z0)*256;c[3]=(X1)*256;c[4]=(Y1)*256;c[5]=(Z1)*256; \
    c[6]=(X2)*256;c[7]=(Y2)*256;c[8]=(Z2)*256;c[9]=(X3)*256;c[10]=(Y3)*256;c[11]=(Z3)*256; fn(ud, tex, sh, c); } while (0)
    for (int x = cx - r; x <= cx + r; x++) for (int y = cy - r; y <= cy + r; y++) for (int z = cz - r; z <= cz + r; z++) {
        if (!rd_is_solid(g, x, y, z)) continue;
        int tex = (y == RD_D * 2 / 3) ? 1 : 0; uint32_t sh;
        if (!rd_is_solid(g, x, y - 1, z) && eye[1] < (float)y) { sh = shade(brightness(g, x, y - 1, z) * 1.0f); Q(x,y,z+1, x,y,z, x+1,y,z, x+1,y,z+1); }
        if (!rd_is_solid(g, x, y + 1, z) && eye[1] > (float)(y + 1)) { sh = shade(brightness(g, x, y, z) * 1.0f); Q(x+1,y+1,z+1, x+1,y+1,z, x,y+1,z, x,y+1,z+1); }
        if (!rd_is_solid(g, x, y, z - 1) && eye[2] < (float)z) { sh = shade(brightness(g, x, y, z - 1) * 0.8f); Q(x,y+1,z, x+1,y+1,z, x+1,y,z, x,y,z); }
        if (!rd_is_solid(g, x, y, z + 1) && eye[2] > (float)(z + 1)) { sh = shade(brightness(g, x, y, z + 1) * 0.8f); Q(x,y+1,z+1, x,y,z+1, x+1,y,z+1, x+1,y+1,z+1); }
        if (!rd_is_solid(g, x - 1, y, z) && eye[0] < (float)x) { sh = shade(brightness(g, x - 1, y, z) * 0.6f); Q(x,y+1,z+1, x,y+1,z, x,y,z, x,y,z+1); }
        if (!rd_is_solid(g, x + 1, y, z) && eye[0] > (float)(x + 1)) { sh = shade(brightness(g, x + 1, y, z) * 0.6f); Q(x+1,y,z+1, x+1,y,z, x+1,y+1,z, x+1,y+1,z+1); }
    }
#undef Q
}

typedef struct { const M3dTarget *tg; const M3dView *v; const M3dTex *t; } RCtx;
static void raster_cb(void *ud, int tex, uint32_t shade_c, const int32_t c[12]) {
    RCtx *r = (RCtx *)ud; int u0 = tex * 16, u1 = u0 + 16; int32_t uv[8] = { u0, 16, u0, 0, u1, 0, u1, 16 }; int32_t col = (int32_t)shade_c;
    m3d_quads(r->tg, r->v, r->t, 1, c, uv, &col);
}
void rd_render(const Rd *g, float alpha, const M3dTex *tex, int w, int h, uint16_t *fb, uint16_t *zb) {
    M3dView v; float eye[3]; RCtx ctx; M3dTarget tg; tg.fb = fb; tg.zb = zb; tg.w = w; tg.h = h;
    v.cy = h * 0.5f; rd_camera(g, alpha, w, &v, eye); ctx.tg = &tg; ctx.v = &v; ctx.t = tex;
    { uint16_t sky = m3d_rgb565(SKY); for (int i = 0; i < w * h; i++) fb[i] = sky; }
    m3d_clear_depth(&tg);
    rd_gen(g, eye, raster_cb, &ctx);
    for (int i = -4; i <= 4; i++) { int x = w / 2 + i, y = h / 2; if (x >= 0 && x < w) fb[y * w + x] = 0xffff; x = w / 2; y = h / 2 + i; if (y >= 0 && y < h) fb[y * w + x] = 0xffff; }
}
