/* Native C port of the J2ME "rd-132211" game logic (level, player physics, raycast, mesh gen). Portable, no libc beyond math/string. */
#ifndef RD_NATIVE_H
#define RD_NATIVE_H
#include <stdint.h>
#include "m3d.h"

#define RD_W 64
#define RD_H 64    /* "height" = z extent in the original */
#define RD_D 64    /* depth   = y extent */

enum { RD_FWD, RD_BACK, RD_LEFT, RD_RIGHT, RD_JUMP, RD_RESET, RD_N_HELD };
enum { RD_TURN_L = 100, RD_TURN_R, RD_LOOK_UP, RD_LOOK_DOWN, RD_BREAK, RD_PLACE };

typedef struct { float x0, y0, z0, x1, y1, z1; } RdBox;
typedef struct { int x, y, z, f; } RdHit;

typedef struct {
    uint8_t bits[RD_W * RD_H * RD_D / 8];     /* 1 bit per block: solid or air */
    uint8_t light[RD_W * RD_H];               /* top solid y per column */
    float xo, yo, zo, x, y, z, xd, yd, zd, yrot, xrot;
    RdBox bb; int on_ground;
    uint8_t held[RD_N_HELD];
    float turn_yaw, turn_pitch;
    RdHit hit; int has_hit;
    uint32_t rnd; /* java.util.Random state (48-bit) */
    float passed; int ticks; float alpha; int64_t last_ms;
    int radius;
    uint64_t seed48;
} Rd;

typedef void (*RdQuadFn)(void *ud, int tex, uint32_t shade, const int32_t c[12]);

void rd_init(Rd *g, int64_t seed);
void rd_key(Rd *g, int key, int down);
void rd_look(Rd *g, float dx, float dy);          /* turn the camera by a touch drag: same units as the turn keys (12 per key tick) */
void rd_tick(Rd *g);
void rd_advance(Rd *g, int64_t now_ms);          /* Timer.advanceTime: sets g->ticks and g->alpha */
void rd_camera(const Rd *g, float alpha, int w, M3dView *out, float *eye);
void rd_gen(const Rd *g, const float eye[3], RdQuadFn fn, void *ud);   /* emit all visible quads */
void rd_render(const Rd *g, float alpha, const M3dTex *tex, int w, int h, uint16_t *fb, uint16_t *zb);
int  rd_is_solid(const Rd *g, int x, int y, int z);
void rd_set_solid(Rd *g, int x, int y, int z, int v);
void rd_recalc_light(Rd *g);
void rd_place_player(Rd *g, float x, float y, float z, float yaw);
#endif
