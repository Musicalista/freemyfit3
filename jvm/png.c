/* png.c: small PNG decoder -> RGB565 pixels + optional 8-bit alpha. Non-interlaced and Adam7; gray/RGB/palette/gray+alpha/RGBA, depths 1-16. */
#include "sys.h"
#include "inflate.h"
#include "png.h"

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int absi(int v) { return v < 0 ? -v : v; }
static uint16_t c565(int r, int g, int b) { return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }

static void unfilter(uint8_t *row, const uint8_t *prev, int n, int bpp, int ft) {
    int i;
    switch (ft) {
    case 1: for (i = bpp; i < n; i++) row[i] = (uint8_t)(row[i] + row[i - bpp]); break;
    case 2: if (prev) for (i = 0; i < n; i++) row[i] = (uint8_t)(row[i] + prev[i]); break;
    case 3: for (i = 0; i < n; i++) { int a = i >= bpp ? row[i - bpp] : 0, b = prev ? prev[i] : 0; row[i] = (uint8_t)(row[i] + ((a + b) >> 1)); } break;
    case 4: for (i = 0; i < n; i++) {
        int a = i >= bpp ? row[i - bpp] : 0, b = prev ? prev[i] : 0, c = (i >= bpp && prev) ? prev[i - bpp] : 0;
        int p = a + b - c, pa = absi(p - a), pb = absi(p - b), pc = absi(p - c);
        int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); row[i] = (uint8_t)(row[i] + pr); } break;
    default: break;
    }
}
int png_decode(const uint8_t *d, uint32_t n, uint16_t **ppx, uint8_t **palpha, int *pw, int *ph) {
    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    uint32_t i, p = 8; int w = 0, h = 0, depth = 0, ctype = 0, inter = 0, npal = 0, ntrns = 0;
    uint8_t pal[768], trns[256]; uint32_t idatlen = 0; uint8_t *idat = 0, *raw = 0; uint16_t *px = 0; uint8_t *al = 0; int ok = 0;
    if (n < 33) return -1;
    for (i = 0; i < 8; i++) if (d[i] != sig[i]) return -1;
    memset(trns, 255, sizeof trns);
    for (uint32_t q = p; q + 12 <= n; ) {                       /* first pass: sizes */
        uint32_t len = be32(d + q); const uint8_t *t = d + q + 4;
        if (q + 12 + len > n) break;
        if (!memcmp(t, "IHDR", 4)) { w = (int)be32(t + 4); h = (int)be32(t + 8); depth = t[12]; ctype = t[13]; inter = t[16]; }
        else if (!memcmp(t, "PLTE", 4)) { npal = (int)(len / 3); if (npal > 256) npal = 256; memcpy(pal, t + 4, (size_t)npal * 3); }
        else if (!memcmp(t, "tRNS", 4)) { ntrns = (int)len > 256 ? 256 : (int)len; if (ctype == 3) memcpy(trns, t + 4, (size_t)ntrns); }
        else if (!memcmp(t, "IDAT", 4)) idatlen += len;
        else if (!memcmp(t, "IEND", 4)) break;
        q += 12 + len;
    }
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || !idatlen) return -1;
    idat = (uint8_t *)malloc(idatlen);
    if (!idat) return -2;
    { uint32_t o = 0; for (uint32_t q = p; q + 12 <= n; ) { uint32_t len = be32(d + q); const uint8_t *t = d + q + 4; if (q + 12 + len > n) break;
        if (!memcmp(t, "IDAT", 4)) { memcpy(idat + o, t + 4, len); o += len; } else if (!memcmp(t, "IEND", 4)) break; q += 12 + len; } }
    int chans = ctype == 0 ? 1 : ctype == 2 ? 3 : ctype == 3 ? 1 : ctype == 4 ? 2 : 4;
    int bpp_bits = chans * depth, bpp = (bpp_bits + 7) / 8; if (bpp < 1) bpp = 1;
    uint32_t rawsize = 0; int passw[7], passh[7], nps = inter ? 7 : 1;
    static const int ox[7] = { 0, 4, 0, 2, 0, 1, 0 }, oy[7] = { 0, 0, 4, 0, 2, 0, 1 }, dx[7] = { 8, 8, 4, 4, 2, 2, 1 }, dy[7] = { 8, 8, 8, 4, 4, 2, 2 };
    for (int s = 0; s < nps; s++) {
        if (inter) { passw[s] = (w - ox[s] + dx[s] - 1) / dx[s]; passh[s] = (h - oy[s] + dy[s] - 1) / dy[s]; if (w <= ox[s]) passw[s] = 0; if (h <= oy[s]) passh[s] = 0; }
        else { passw[s] = w; passh[s] = h; }
        if (passw[s] > 0 && passh[s] > 0) rawsize += (uint32_t)passh[s] * (1 + (uint32_t)((passw[s] * bpp_bits + 7) / 8));
    }
    raw = (uint8_t *)malloc(rawsize + 8);
    if (!raw) { free(idat); return -2; }
    if (idatlen < 6 || inflate_raw(idat + 2, idatlen - 2, raw, rawsize) < 0) { free(idat); free(raw); return -3; }
    free(idat);
    px = (uint16_t *)malloc((uint32_t)w * (uint32_t)h * 2);
    int has_alpha = (ctype == 4 || ctype == 6 || (ctype == 3 && ntrns > 0) || ((ctype == 0 || ctype == 2) && ntrns > 0));
    if (has_alpha) al = (uint8_t *)malloc((uint32_t)w * (uint32_t)h);
    if (!px || (has_alpha && !al)) { free(raw); free(px); free(al); return -2; }
    { uint32_t off = 0;
      for (int s = 0; s < nps; s++) {
        int pw2 = passw[s], ph2 = passh[s]; if (pw2 <= 0 || ph2 <= 0) continue;
        int stride = (pw2 * bpp_bits + 7) / 8; uint8_t *prev = 0;
        for (int y = 0; y < ph2; y++) {
            uint8_t *row = raw + off + (uint32_t)y * (uint32_t)(stride + 1); unfilter(row + 1, prev, stride, bpp, row[0]); prev = row + 1;
            for (int x = 0; x < pw2; x++) {
                int r = 0, g = 0, b = 0, a = 255; const uint8_t *q = row + 1;
                if (depth == 8) {
                    if (ctype == 0) { r = g = b = q[x]; }
                    else if (ctype == 2) { r = q[3 * x]; g = q[3 * x + 1]; b = q[3 * x + 2]; }
                    else if (ctype == 3) { int k = q[x]; r = pal[3 * k]; g = pal[3 * k + 1]; b = pal[3 * k + 2]; a = trns[k]; }
                    else if (ctype == 4) { r = g = b = q[2 * x]; a = q[2 * x + 1]; }
                    else { r = q[4 * x]; g = q[4 * x + 1]; b = q[4 * x + 2]; a = q[4 * x + 3]; }
                } else if (depth == 16) {
                    if (ctype == 0) r = g = b = q[2 * x]; else if (ctype == 2) { r = q[6 * x]; g = q[6 * x + 2]; b = q[6 * x + 4]; }
                    else if (ctype == 4) { r = g = b = q[4 * x]; a = q[4 * x + 2]; } else { r = q[8 * x]; g = q[8 * x + 2]; b = q[8 * x + 4]; a = q[8 * x + 6]; }
                } else {
                    int bit = x * depth, v = (q[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
                    if (ctype == 3) { r = pal[3 * v]; g = pal[3 * v + 1]; b = pal[3 * v + 2]; a = trns[v]; } else { r = g = b = v * 255 / ((1 << depth) - 1); }
                }
                int X = inter ? ox[s] + x * dx[s] : x, Y = inter ? oy[s] + y * dy[s] : y;
                px[Y * w + X] = c565(r, g, b); if (al) al[Y * w + X] = (uint8_t)a;
            }
        }
        off += (uint32_t)ph2 * (uint32_t)(stride + 1);
      } }
    free(raw); ok = 1; (void)ok;
    *ppx = px; *palpha = al; *pw = w; *ph = h; return 0;
}
