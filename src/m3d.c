#include "m3d.h"

void m3d_clear_depth(const M3dTarget *t) {
    uint32_t *p = (uint32_t *)t->zb; int n = (t->w * t->h) >> 1;
    for (int i = 0; i < n; i++) p[i] = 0;
    if ((t->w * t->h) & 1) t->zb[t->w * t->h - 1] = 0;
}
uint16_t m3d_rgb565(uint32_t c) { return (uint16_t)(((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x1f)); }

#ifndef M3D_NO_BMP
#include <stdlib.h>
#include <string.h>
int m3d_parse_bmp(const uint8_t *b, uint32_t len, M3dTex *o) {
    if (len < 54 || b[0] != 'B' || b[1] != 'M') return -1;
    uint32_t off = b[10] | (b[11] << 8) | (b[12] << 16) | ((uint32_t)b[13] << 24);
    int w = (int)(b[18] | (b[19] << 8) | (b[20] << 16)), h = (int)(b[22] | (b[23] << 8) | (b[24] << 16));
    int bpp = b[28] | (b[29] << 8), hdr = b[14] | (b[15] << 8), ncol = b[46] | (b[47] << 8); if (!ncol) ncol = 256;
    if (bpp != 8 || w <= 0 || h <= 0 || off + (uint32_t)w * h > len) return -2;
    uint16_t pal[256]; memset(pal, 0, sizeof pal);
    for (int i = 0; i < ncol && i < 256; i++) { const uint8_t *e = b + 14 + hdr + i * 4; pal[i] = m3d_rgb565(((uint32_t)e[2] << 16) | (e[1] << 8) | e[0]); }
    uint16_t *pix = malloc((size_t)w * h * 2);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) pix[y * w + x] = pal[b[off + (size_t)(h - 1 - y) * ((w + 3) & ~3) + x]];
    o->pix = pix; o->w = w; o->h = h; return 0;
}
#endif

typedef struct { float x, y, z, u, v; } Vtx;

static int clip_near(const Vtx *in, int n, float zn, Vtx *out) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        const Vtx *a = &in[i], *b = &in[(i + 1) % n]; int ain = a->z >= zn, bin = b->z >= zn;
        if (ain) out[m++] = *a;
        if (ain != bin) { float t = (zn - a->z) / (b->z - a->z);
            out[m].x = a->x + (b->x - a->x) * t; out[m].y = a->y + (b->y - a->y) * t; out[m].z = zn;
            out[m].u = a->u + (b->u - a->u) * t; out[m].v = a->v + (b->v - a->v) * t; m++; }
    }
    return m;
}
static int ceili(float f) { int i = (int)f; return (f > (float)i) ? i + 1 : i; }

static void tri(const M3dTarget *Tg, const M3dView *V, const M3dTex *T, uint32_t gm, const Vtx *p0, const Vtx *p1, const Vtx *p2) {
    float iz0 = 1.0f / p0->z, iz1 = 1.0f / p1->z, iz2 = 1.0f / p2->z;
    float X0 = V->cx + V->f * p0->x * iz0, Y0 = V->cy + V->f * p0->y * iz0;
    float X1 = V->cx + V->f * p1->x * iz1, Y1 = V->cy + V->f * p1->y * iz1;
    float X2 = V->cx + V->f * p2->x * iz2, Y2 = V->cy + V->f * p2->y * iz2;
    float area = (X1 - X0) * (Y2 - Y0) - (X2 - X0) * (Y1 - Y0);
    if (area > -1e-4f && area < 1e-4f) return;
    float inv = 1.0f / area;
    float ymn = Y0 < Y1 ? (Y0 < Y2 ? Y0 : Y2) : (Y1 < Y2 ? Y1 : Y2), ymx = Y0 > Y1 ? (Y0 > Y2 ? Y0 : Y2) : (Y1 > Y2 ? Y1 : Y2);
    int ya = ceili(ymn - 0.5f), yb = ceili(ymx - 0.5f) - 1; if (ya < 0) ya = 0; if (yb > Tg->h - 1) yb = Tg->h - 1;
    if (ya > yb) return;
    float uz0 = p0->u * iz0, uz1 = p1->u * iz1, uz2 = p2->u * iz2, vz0 = p0->v * iz0, vz1 = p1->v * iz1, vz2 = p2->v * iz2;
#define GX(a0, a1, a2) (((a1 - a0) * (Y2 - Y0) - (a2 - a0) * (Y1 - Y0)) * inv)
#define GY(a0, a1, a2) (((a2 - a0) * (X1 - X0) - (a1 - a0) * (X2 - X0)) * inv)
    float gzx = GX(iz0, iz1, iz2), gzy = GY(iz0, iz1, iz2), gux = GX(uz0, uz1, uz2), guy = GY(uz0, uz1, uz2), gvx = GX(vz0, vz1, vz2), gvy = GY(vz0, vz1, vz2);
    float ex[3] = { X0, X1, X2 }, ey[3] = { Y0, Y1, Y2 };
    float kd = 65535.0f * V->znear; int tw = T->w, th = T->h; uint32_t mm = (gm & 0xff) + 1;
    for (int y = ya; y <= yb; y++) {
        float py = y + 0.5f, xl = 1e30f, xr = -1e30f;
        for (int e = 0; e < 3; e++) {
            int f = e == 2 ? 0 : e + 1; float a = ey[e], b = ey[f];
            if ((a <= py && py < b) || (b <= py && py < a)) { float x = ex[e] + (py - a) * (ex[f] - ex[e]) / (b - a); if (x < xl) xl = x; if (x > xr) xr = x; }
        }
        if (xl > xr) continue;
        int xs = ceili(xl - 0.5f), xe = ceili(xr - 0.5f) - 1; if (xs < 0) xs = 0; if (xe > Tg->w - 1) xe = Tg->w - 1;
        if (xs > xe) continue;
        float dx = (xs + 0.5f) - X0, dy = py - Y0;
        float iz = iz0 + gzx * dx + gzy * dy, uz = uz0 + gux * dx + guy * dy, vz = vz0 + gvx * dx + gvy * dy;
        uint16_t *fr = Tg->fb + y * Tg->w, *zr = Tg->zb + y * Tg->w;
        for (int x = xs; x <= xe;) {
            int n = xe - x + 1; if (n > 8) n = 8;
            float r0 = 1.0f / iz, u0 = uz * r0, v0 = vz * r0; float izn = iz + gzx * (n - 1), r1 = 1.0f / izn;
            float u1 = (uz + gux * (n - 1)) * r1, v1 = (vz + gvx * (n - 1)) * r1;
            float du = n > 1 ? (u1 - u0) / (float)(n - 1) : 0.0f, dv = n > 1 ? (v1 - v0) / (float)(n - 1) : 0.0f;
            for (int k = 0; k < n; k++, x++) {
                int key = (int)(iz * kd); if (key > 65535) key = 65535;
                if (key > (int)zr[x]) {
                    zr[x] = (uint16_t)key;
                    int tu = (int)u0, tv = (int)v0; if (tu < 0) tu = 0; else if (tu >= tw) tu = tw - 1; if (tv < 0) tv = 0; else if (tv >= th) tv = th - 1;
                    uint32_t c = T->pix[tv * tw + tu];
                    fr[x] = (uint16_t)((((c >> 11) * mm >> 8) << 11) | ((((c >> 5) & 63) * mm >> 8) << 5) | ((c & 31) * mm >> 8));
                }
                iz += gzx; u0 += du; v0 += dv;
            }
            uz += gux * n; vz += gvx * n;
        }
    }
}

void m3d_quads(const M3dTarget *Tg, const M3dView *v, const M3dTex *t, int n, const int32_t *coords, const int32_t *uv, const int32_t *col) {
    const float *m = v->m;
    for (int q = 0; q < n; q++) {
        Vtx a[4], b[8]; uint32_t gray = col ? (uint32_t)col[q] : 0xffffff;
        for (int k = 0; k < 4; k++) {
            float x = (float)coords[q * 12 + k * 3], y = (float)coords[q * 12 + k * 3 + 1], z = (float)coords[q * 12 + k * 3 + 2];
            a[k].x = m[0] * x + m[1] * y + m[2] * z + m[3]; a[k].y = m[4] * x + m[5] * y + m[6] * z + m[7]; a[k].z = m[8] * x + m[9] * y + m[10] * z + m[11];
            a[k].u = (float)uv[q * 8 + k * 2]; a[k].v = (float)uv[q * 8 + k * 2 + 1];
        }
        int cnt = clip_near(a, 4, v->znear, b);
        for (int i = 1; i + 1 < cnt; i++) tri(Tg, v, t, gray, &b[0], &b[i], &b[i + 1]);
    }
}
