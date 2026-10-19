/* Software rasterizer for Micro3D-style QUADS: perspective-correct (per 8px span) textured triangles,
 * per-face gray modulate, RGB565 colour + 16-bit depth. No libc. */
#ifndef M3D_H
#define M3D_H
#include <stdint.h>

typedef struct { const uint16_t *pix; int w, h; } M3dTex;                              /* RGB565 texels */
typedef struct { float m[12]; float cx, cy, f, znear, zfar; } M3dView;                 /* world->view 3x4, y down, z forward */

typedef struct { uint16_t *fb, *zb; int w, h; } M3dTarget;   /* colour + depth targets (caller-owned: no globals, cave is read-only) */
void m3d_clear_depth(const M3dTarget *t);
/* coords: n*4*3 ints (model space), uv: n*4*2 ints (texels), col: n ints (0xRRGGBB gray) or NULL */
void m3d_quads(const M3dTarget *tg, const M3dView *v, const M3dTex *t, int n, const int32_t *coords, const int32_t *uv, const int32_t *col);
uint16_t m3d_rgb565(uint32_t rgb888);
#ifndef M3D_NO_BMP
/* PC only: parse 8-bit BMP (bottom-up) into malloc'd RGB565 texture */
int m3d_parse_bmp(const uint8_t *bmp, uint32_t len, M3dTex *out);
#endif
#endif
