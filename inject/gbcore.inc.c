/* ================= Game Boy / Game Boy Color core (original code) =================
 * Freestanding: no libc, no globals (all state lives in one malloc'd GB struct; the cave is read-only flash).
 * CPU (all opcodes), timers, interrupts, MBC1/2/3/5, OAM/HDMA DMA, a scanline PPU for DMG and CGB (2 VRAM banks, 8 WRAM banks, palettes,
 * attributes, double speed). No sound. The PPU writes each finished scanline through gb_emit() (supplied by the front end).
 * Needs: u8 u16 u32, MALLOC/FREE, C(r,g,b). */
#define GB_VRAM 0x4000
#define GB_WRAM 0x8000
typedef struct {
    u8 r[8], f; u16 sp, pc;                       /* r: B C D E H L (unused) A */
    u8 ime, ei, halt, cgb, dbl, mbc, bat, dirty, draw, fdone, mode, wlive;
    u8 *rom, *eram; u32 romsz, ersz, rofs0, rofs, raofs; int nbanks;
    int b1, b2, mbmode, rsel, ramen, vbk, wbk, ly, lc, wly, tcnt, divc, ccount;
    int hdma_on, hdma_blk; u32 hsrc, hdst; u8 rtc[5];
    u8 jd, jb;                                    /* joypad: d-pad (right 1 left 2 up 4 down 8), buttons (A 1 B 2 Select 4 Start 8), pressed = 1 */
    u8 io[128], hram[128], oam[160], bgi[160], bgp[64], obp[64];
    u16 cbg[32], cob[32], line[160];
    u8 vram[GB_VRAM], wram[GB_WRAM];
    void *emit_ctx;
} GB;

static void gb_emit(GB *g, int ly);                /* front end: a scanline is ready in g->line */

static u8 gb_rd(GB *g, u16 a);
static void gb_wr(GB *g, u16 a, u8 v);

static const u8 gb_cyc_tab[128] = {
    1, 3, 2, 2, 1, 1, 2, 1, 5, 2, 2, 2, 1, 1, 2, 1,  1, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,
    2, 3, 2, 2, 1, 1, 2, 1, 2, 2, 2, 2, 1, 1, 2, 1,  2, 3, 2, 2, 3, 3, 3, 1, 2, 2, 2, 2, 1, 1, 2, 1,
    2, 3, 3, 4, 3, 4, 2, 4, 2, 4, 3, 1, 3, 6, 2, 4,  2, 3, 3, 1, 3, 4, 2, 4, 2, 4, 3, 1, 3, 1, 2, 4,
    3, 3, 2, 1, 1, 4, 2, 4, 4, 1, 4, 1, 1, 1, 2, 4,  3, 3, 2, 1, 1, 4, 2, 4, 3, 2, 4, 1, 1, 1, 2, 4 };
static int gb_cyc(u8 op) {
    if (op >= 0x40 && op < 0xC0) { if (op == 0x76) return 1; return ((op & 7) == 6 || (op < 0x80 && ((op >> 3) & 7) == 6)) ? 2 : 1; }
    return gb_cyc_tab[op < 0x40 ? op : op - 0x80];
}

/* ---------------- cartridge banking ---------------- */
static void gb_banks(GB *g) {
    int rb, nb = g->nbanks;
    if (g->mbc == 1) { rb = g->b1 | (g->b2 << 5); g->rofs0 = g->mbmode ? (u32)(((g->b2 << 5) & (nb - 1)) * 0x4000) : 0; }
    else if (g->mbc == 5) { rb = g->b1 | (g->b2 << 8); g->rofs0 = 0; }
    else { rb = g->b1; g->rofs0 = 0; }
    g->rofs = (u32)((rb & (nb - 1)) * 0x4000) - 0x4000;
    int ramb = g->mbc == 1 ? (g->mbmode ? g->b2 : 0) : g->mbc == 3 ? (g->rsel & 3) : g->mbc == 5 ? (g->rsel & 15) : 0;
    g->raofs = (u32)ramb * 0x2000;
}
static void gb_mbc_wr(GB *g, u16 a, u8 v) {
    switch (a >> 13) {
    case 0: if (g->mbc == 2 && (a & 0x100)) g->b1 = v & 15 ? v & 15 : 1; else g->ramen = (v & 15) == 10; break;
    case 1:
        if (g->mbc == 1) g->b1 = (v & 31) ? (v & 31) : 1;
        else if (g->mbc == 3) g->b1 = (v & 127) ? (v & 127) : 1;
        else if (g->mbc == 2) { if (a & 0x100) g->b1 = (v & 15) ? (v & 15) : 1; }
        else if (g->mbc == 5) { if (a < 0x3000) g->b1 = v; else g->b2 = v & 1; }
        break;
    case 2: if (g->mbc == 1) g->b2 = v & 3; else g->rsel = v; break;
    case 3: if (g->mbc == 1) g->mbmode = v & 1; break;
    }
    gb_banks(g);
}

/* ---------------- memory ---------------- */
static u8 gb_joy(GB *g) {
    u8 sel = g->io[0], v = (u8)(0xC0 | (sel & 0x30) | 0x0F);
    if (!(sel & 0x10)) v &= (u8)~(g->jd & 15);
    if (!(sel & 0x20)) v &= (u8)~(g->jb & 15);
    return v;
}
static u8 gb_ioR(GB *g, int r) {
    switch (r) {
    case 0x00: return gb_joy(g);
    case 0x04: return (u8)(g->divc >> 8);
    case 0x0F: return g->io[0x0F] | 0xE0;
    case 0x41: return (u8)(0x80 | (g->io[0x41] & 0x78) | (g->ly == g->io[0x45] ? 4 : 0) | ((g->io[0x40] & 0x80) ? g->mode : 0));
    case 0x44: return (u8)g->ly;
    case 0x4D: return (u8)(g->cgb ? ((g->dbl << 7) | 0x7E | (g->io[0x4D] & 1)) : 0xFF);
    case 0x4F: return (u8)(g->cgb ? (0xFE | g->vbk) : 0xFF);
    case 0x55: return (u8)(g->hdma_on ? (g->hdma_blk - 1) : 0xFF);
    case 0x69: return g->bgp[g->io[0x68] & 63];
    case 0x6B: return g->obp[g->io[0x6A] & 63];
    case 0x70: return (u8)(g->cgb ? (0xF8 | g->wbk) : 0xFF);
    }
    return g->io[r];
}
static void gb_pal(GB *g, int obj, u8 v) {
    u8 *p = obj ? g->obp : g->bgp; u16 *c = obj ? g->cob : g->cbg; int idx = g->io[obj ? 0x6A : 0x68], i = idx & 63;
    p[i] = v; { int e = i >> 1; u32 w = (u32)p[e * 2] | ((u32)p[e * 2 + 1] << 8); c[e] = (u16)(((w & 31) << 11) | (((w >> 5) & 31) << 6) | ((w >> 10) & 31)); }
    if (idx & 0x80) g->io[obj ? 0x6A : 0x68] = (u8)(0x80 | ((i + 1) & 63));
}
static void gb_ioW(GB *g, int r, u8 v) {
    switch (r) {
    case 0x00: g->io[0] = (u8)(v & 0x30); return;
    case 0x04: g->divc = 0; return;
    case 0x40:
        if (!(v & 0x80) && (g->io[0x40] & 0x80)) { g->ly = 0; g->lc = 0; g->mode = 0; g->wly = 0; }
        if ((v & 0x80) && !(g->io[0x40] & 0x80)) { g->lc = 0; g->ly = 0; g->mode = 0; }
        break;
    case 0x41: g->io[0x41] = (u8)(v & 0x78); return;
    case 0x44: return;
    case 0x46: { u32 s = (u32)v << 8; for (int i = 0; i < 160; i++) g->oam[i] = gb_rd(g, (u16)(s + (u32)i)); break; }
    case 0x4D: g->io[0x4D] = (u8)(v & 1); return;
    case 0x4F: if (g->cgb) g->vbk = v & 1; return;
    case 0x51: g->hsrc = (g->hsrc & 0xFF) | ((u32)v << 8); return;
    case 0x52: g->hsrc = (g->hsrc & 0xFF00) | (v & 0xF0u); return;
    case 0x53: g->hdst = (g->hdst & 0xFF) | ((u32)(v & 0x1F) << 8); return;
    case 0x54: g->hdst = (g->hdst & 0xFF00) | (v & 0xF0u); return;
    case 0x55:
        if (!g->cgb) return;
        if (g->hdma_on && !(v & 0x80)) { g->hdma_on = 0; return; }                       /* cancel */
        g->hdma_blk = (v & 0x7F) + 1;
        if (v & 0x80) { g->hdma_on = 1; return; }
        for (int i = 0; i < g->hdma_blk * 16; i++) { g->vram[(g->vbk << 13) + ((g->hdst + (u32)i) & 0x1FFF)] = gb_rd(g, (u16)(g->hsrc + (u32)i)); }
        g->hdma_blk = 0; g->hdma_on = 0; return;
    case 0x69: gb_pal(g, 0, v); return;
    case 0x6B: gb_pal(g, 1, v); return;
    case 0x70: if (g->cgb) { g->wbk = v & 7; if (!g->wbk) g->wbk = 1; } return;
    }
    g->io[r] = v;
}
static u8 gb_rd(GB *g, u16 a) {
    switch (a >> 12) {
    case 0: case 1: case 2: case 3: return g->rom[g->rofs0 + a];
    case 4: case 5: case 6: case 7: return g->rom[g->rofs + a];
    case 8: case 9: return g->vram[(g->vbk << 13) + (a & 0x1FFF)];
    case 10: case 11:
        if (!g->ramen) return 0xFF;
        if (g->mbc == 3 && g->rsel >= 8) return g->rtc[(g->rsel - 8) % 5];
        if (!g->ersz) return 0xFF;
        if (g->mbc == 2) return (u8)(g->eram[a & 0x1FF] | 0xF0);
        { u32 i = g->raofs + (a & 0x1FFF); if (i >= g->ersz) i %= g->ersz; return g->eram[i]; }
    case 12: case 14: return g->wram[a & 0xFFF];
    case 13: return g->wram[(g->wbk << 12) + (a & 0xFFF)];
    }
    if (a < 0xFE00) return g->wram[(g->wbk << 12) + (a & 0xFFF)];
    if (a < 0xFEA0) return g->oam[a - 0xFE00];
    if (a < 0xFF00) return 0;
    if (a < 0xFF80) return gb_ioR(g, a & 0x7F);
    if (a < 0xFFFF) return g->hram[a & 0x7F];
    return g->io[0x7F];                                                                 /* IE is kept in io[0x7F] */
}
static void gb_wr(GB *g, u16 a, u8 v) {
    switch (a >> 12) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: gb_mbc_wr(g, a, v); return;
    case 8: case 9: g->vram[(g->vbk << 13) + (a & 0x1FFF)] = v; return;
    case 10: case 11:
        if (!g->ramen) return;
        if (g->mbc == 3 && g->rsel >= 8) { g->rtc[(g->rsel - 8) % 5] = v; return; }
        if (!g->ersz) return;
        if (g->mbc == 2) { g->eram[a & 0x1FF] = (u8)(v & 15); g->dirty = 1; return; }
        { u32 i = g->raofs + (a & 0x1FFF); if (i >= g->ersz) i %= g->ersz; if (g->eram[i] != v) { g->eram[i] = v; g->dirty = 1; } }
        return;
    case 12: case 14: g->wram[a & 0xFFF] = v; return;
    case 13: g->wram[(g->wbk << 12) + (a & 0xFFF)] = v; return;
    }
    if (a < 0xFE00) { g->wram[(g->wbk << 12) + (a & 0xFFF)] = v; return; }
    if (a < 0xFEA0) { g->oam[a - 0xFE00] = v; return; }
    if (a < 0xFF00) return;
    if (a < 0xFF80) { gb_ioW(g, a & 0x7F, v); return; }
    if (a < 0xFFFF) { g->hram[a & 0x7F] = v; return; }
    g->io[0x7F] = v;
}

/* ---------------- PPU ---------------- */
static u16 gb_dmg(int s) { static const u16 pal[4] = { C(224, 248, 208), C(136, 192, 112), C(52, 104, 86), C(8, 24, 32) }; return pal[s & 3]; }
static void gb_line(GB *g, int ly) {
    u8 lcdc = g->io[0x40]; int cgb = g->cgb, x;
    u16 dmgbg[4]; for (int i = 0; i < 4; i++) dmgbg[i] = gb_dmg(g->io[0x47] >> (i * 2));
    if (!(lcdc & 1) && !cgb) { for (x = 0; x < 160; x++) { g->line[x] = dmgbg[0]; g->bgi[x] = 0; } }
    else {
        int scx = g->io[0x43], scy = g->io[0x42], wy = g->io[0x4A], wx = g->io[0x4B] - 7;
        int win = (lcdc & 0x20) && ly >= wy && wx < 160, usedwin = 0;
        x = 0;
        while (x < 160) {
            int uw = win && x >= wx, px, py, base;
            if (uw) { px = x - wx; py = g->wly; base = (lcdc & 0x40) ? 0x1C00 : 0x1800; usedwin = 1; }
            else { px = (x + scx) & 255; py = (ly + scy) & 255; base = (lcdc & 8) ? 0x1C00 : 0x1800; }
            int mi = base + (py >> 3) * 32 + (px >> 3), tile = g->vram[mi], attr = cgb ? g->vram[0x2000 + mi] : 0;
            int t = (lcdc & 0x10) ? tile * 16 : 0x1000 + (int)(signed char)tile * 16, row = py & 7;
            if (attr & 0x40) row = 7 - row;
            int bank = (attr & 8) ? 0x2000 : 0, lo = g->vram[bank + t + row * 2], hi = g->vram[bank + t + row * 2 + 1];
            int b0 = px & 7, n = 8 - b0;
            if (x + n > 160) n = 160 - x;
            if (!uw && win && x < wx && x + n > wx) n = wx - x;
            for (int i = 0; i < n; i++) {
                int bit = (attr & 0x20) ? b0 + i : 7 - (b0 + i), ci = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                g->line[x + i] = cgb ? g->cbg[(attr & 7) * 4 + ci] : dmgbg[ci];
                g->bgi[x + i] = (u8)(ci | ((attr & 0x80) ? 0x80 : 0));
            }
            x += n;
        }
        if (usedwin) g->wly++;
    }
    if (lcdc & 2) {
        int h = (lcdc & 4) ? 16 : 8, n = 0; u8 sel[10], kx[10];
        for (int i = 0; i < 40 && n < 10; i++) { int y = g->oam[i * 4] - 16; if (ly >= y && ly < y + h) { sel[n] = (u8)i; kx[n] = g->oam[i * 4 + 1]; n++; } }
        if (!cgb) for (int i = 1; i < n; i++) { u8 s = sel[i], k = kx[i]; int j = i - 1; while (j >= 0 && kx[j] > k) { sel[j + 1] = sel[j]; kx[j + 1] = kx[j]; j--; } sel[j + 1] = s; kx[j + 1] = k; }
        int master = cgb && !(lcdc & 1);                                                  /* CGB: BG master priority off = sprites always on top */
        for (int s = n - 1; s >= 0; s--) {
            u8 *o = g->oam + sel[s] * 4; int y = o[0] - 16, sx = o[1] - 8, tile = o[2], attr = o[3], row = ly - y;
            if (attr & 0x40) row = h - 1 - row;
            if (h == 16) { tile &= 0xFE; if (row >= 8) { tile++; row -= 8; } }
            int bank = (cgb && (attr & 8)) ? 0x2000 : 0, lo = g->vram[bank + tile * 16 + row * 2], hi = g->vram[bank + tile * 16 + row * 2 + 1];
            u8 dp = (attr & 0x10) ? g->io[0x49] : g->io[0x48];
            for (int i = 0; i < 8; i++) {
                int xx = sx + i; if (xx < 0 || xx >= 160) continue;
                int bit = (attr & 0x20) ? i : 7 - i, ci = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                if (!ci) continue;
                if (!master && g->bgi[xx] != 0 && g->bgi[xx] != 0x80 && ((attr & 0x80) || (cgb && (g->bgi[xx] & 0x80)))) continue;
                g->line[xx] = cgb ? g->cob[(attr & 7) * 4 + ci] : gb_dmg(dp >> (ci * 2));
            }
        }
    }
}
static void gb_mode(GB *g) {
    int d = g->dbl, m;
    if (g->ly >= 144) m = 1; else m = g->lc < (80 << d) ? 2 : g->lc < (252 << d) ? 3 : 0;
    if (m == g->mode) return;
    g->mode = (u8)m;
    if (m == 0) {
        if (g->io[0x41] & 8) g->io[0x0F] |= 2;
        if (g->draw) { gb_line(g, g->ly); gb_emit(g, g->ly); } else if ((g->io[0x40] & 0x20) && g->ly >= g->io[0x4A] && g->io[0x4B] < 167) g->wly++;
        if (g->hdma_on) {
            for (int i = 0; i < 16; i++) { g->vram[(g->vbk << 13) + ((g->hdst + (u32)i) & 0x1FFF)] = gb_rd(g, (u16)(g->hsrc + (u32)i)); }
            g->hsrc += 16; g->hdst += 16; if (--g->hdma_blk <= 0) { g->hdma_on = 0; g->hdma_blk = 0; }
        }
    } else if (m == 2) { if (g->io[0x41] & 0x20) g->io[0x0F] |= 2; }
    else if (m == 1) { g->io[0x0F] |= 1; if (g->io[0x41] & 0x10) g->io[0x0F] |= 2; g->fdone = 1; }
}
static void gb_time(GB *g, int cyc) {
    g->divc = (g->divc + cyc) & 0xFFFF;
    if (g->io[0x07] & 4) {
        static const int per[4] = { 1024, 16, 64, 256 };
        g->tcnt += cyc; int p = per[g->io[0x07] & 3];
        while (g->tcnt >= p) { g->tcnt -= p; if (++g->io[0x05] == 0) { g->io[0x05] = g->io[0x06]; g->io[0x0F] |= 4; } }
    }
    if (!(g->io[0x40] & 0x80)) { g->ccount += cyc; if (g->ccount >= (70224 << g->dbl)) { g->ccount = 0; g->fdone = 1; } return; }
    g->lc += cyc;
    if (g->lc >= (456 << g->dbl)) {
        g->lc -= 456 << g->dbl;
        if (++g->ly == 154) { g->ly = 0; g->wly = 0; }
        if (g->ly == g->io[0x45] && (g->io[0x41] & 0x40)) g->io[0x0F] |= 2;
    }
    gb_mode(g);
}

/* ---------------- CPU ---------------- */
#define FZ 0x80
#define FN 0x40
#define FH 0x20
#define FC 0x10
static u8 gb_get(GB *g, int i) { return i == 6 ? gb_rd(g, (u16)((g->r[4] << 8) | g->r[5])) : g->r[i]; }
static void gb_set(GB *g, int i, u8 v) { if (i == 6) gb_wr(g, (u16)((g->r[4] << 8) | g->r[5]), v); else g->r[i] = v; }
static u16 gb_pair(GB *g, int k) { return k == 3 ? g->sp : (u16)((g->r[k * 2] << 8) | g->r[k * 2 + 1]); }
static void gb_setpair(GB *g, int k, u16 v) { if (k == 3) g->sp = v; else { g->r[k * 2] = (u8)(v >> 8); g->r[k * 2 + 1] = (u8)v; } }
static u8 gb_f8(GB *g) { return gb_rd(g, g->pc++); }
static u16 gb_f16(GB *g) { u16 l = gb_rd(g, g->pc++); return (u16)(l | (gb_rd(g, g->pc++) << 8)); }
static void gb_push(GB *g, u16 v) { gb_wr(g, --g->sp, (u8)(v >> 8)); gb_wr(g, --g->sp, (u8)v); }
static u16 gb_pop(GB *g) { u16 l = gb_rd(g, g->sp++); return (u16)(l | (gb_rd(g, g->sp++) << 8)); }
static void gb_alu(GB *g, int op, u8 v) {
    int a = g->r[7], r, c = (g->f >> 4) & 1;
    switch (op) {
    case 0: case 1: { int cc = op ? c : 0; r = a + v + cc; g->f = (u8)(((r & 255) == 0 ? FZ : 0) | (((a & 15) + (v & 15) + cc) > 15 ? FH : 0) | (r > 255 ? FC : 0)); g->r[7] = (u8)r; break; }
    case 2: case 3: case 7: { int cc = op == 3 ? c : 0; r = a - v - cc; g->f = (u8)(FN | ((r & 255) == 0 ? FZ : 0) | (((a & 15) - (v & 15) - cc) < 0 ? FH : 0) | (r < 0 ? FC : 0)); if (op != 7) g->r[7] = (u8)r; break; }
    case 4: g->r[7] = (u8)(a & v); g->f = (u8)((g->r[7] ? 0 : FZ) | FH); break;
    case 5: g->r[7] = (u8)(a ^ v); g->f = (u8)(g->r[7] ? 0 : FZ); break;
    case 6: g->r[7] = (u8)(a | v); g->f = (u8)(g->r[7] ? 0 : FZ); break;
    }
}
static int gb_cb(GB *g) {
    u8 op = gb_f8(g); int k = op & 7, y = (op >> 3) & 7, v = gb_get(g, k), c = (g->f >> 4) & 1, m = k == 6 ? 4 : 2;
    switch (op >> 6) {
    case 0: {
        int nc;
        switch (y) {
        case 0: nc = v >> 7; v = (v << 1) | nc; break;
        case 1: nc = v & 1; v = (v >> 1) | (nc << 7); break;
        case 2: nc = v >> 7; v = (v << 1) | c; break;
        case 3: nc = v & 1; v = (v >> 1) | (c << 7); break;
        case 4: nc = v >> 7; v <<= 1; break;
        case 5: nc = v & 1; v = (v >> 1) | (v & 0x80); break;
        case 6: nc = 0; v = (v >> 4) | (v << 4); break;
        default: nc = v & 1; v >>= 1; break;
        }
        v &= 255; g->f = (u8)((v ? 0 : FZ) | (nc ? FC : 0)); gb_set(g, k, (u8)v); break; }
    case 1: g->f = (u8)((g->f & FC) | FH | ((v >> y) & 1 ? 0 : FZ)); if (k == 6) m = 3; break;
    case 2: gb_set(g, k, (u8)(v & ~(1 << y))); break;
    default: gb_set(g, k, (u8)(v | (1 << y))); break;
    }
    return m;
}
static int gb_exec(GB *g) {
    u8 op = gb_f8(g); int m = gb_cyc(op), x = op >> 6, y = (op >> 3) & 7, z = op & 7;
    if (x == 1) { if (op != 0x76) gb_set(g, y, gb_get(g, z)); else g->halt = 1; return m * 4; }
    if (x == 2) { gb_alu(g, y, gb_get(g, z)); return m * 4; }
    if (x == 0) {
        int p = y >> 1;
        switch (z) {
        case 0:
            if (y == 0) break;
            if (y == 1) { u16 a = gb_f16(g); gb_wr(g, a, (u8)g->sp); gb_wr(g, (u16)(a + 1), (u8)(g->sp >> 8)); break; }
            if (y == 2) { gb_f8(g); if (g->io[0x4D] & 1) { g->dbl ^= 1; g->io[0x4D] = 0; g->divc = 0; } break; }
            { signed char d = (signed char)gb_f8(g);
              if (y == 3 || (y == 4 && !(g->f & FZ)) || (y == 5 && (g->f & FZ)) || (y == 6 && !(g->f & FC)) || (y == 7 && (g->f & FC))) { g->pc = (u16)(g->pc + d); m++; } }
            break;
        case 1:
            if (!(y & 1)) gb_setpair(g, p, gb_f16(g));
            else { u32 h = gb_pair(g, 2), v = gb_pair(g, p), r = h + v; g->f = (u8)((g->f & FZ) | (((h & 0xFFF) + (v & 0xFFF)) > 0xFFF ? FH : 0) | (r > 0xFFFF ? FC : 0)); gb_setpair(g, 2, (u16)r); }
            break;
        case 2: {
            u16 a = p == 0 ? gb_pair(g, 0) : p == 1 ? gb_pair(g, 1) : gb_pair(g, 2);
            if (y & 1) g->r[7] = gb_rd(g, a); else gb_wr(g, a, g->r[7]);
            if (p == 2) gb_setpair(g, 2, (u16)(a + 1)); else if (p == 3) gb_setpair(g, 2, (u16)(a - 1));
            break; }
        case 3: gb_setpair(g, p, (u16)(gb_pair(g, p) + ((y & 1) ? -1 : 1))); break;
        case 4: { u8 v = (u8)(gb_get(g, y) + 1); g->f = (u8)((g->f & FC) | (v ? 0 : FZ) | ((v & 15) == 0 ? FH : 0)); gb_set(g, y, v); break; }
        case 5: { u8 v = (u8)(gb_get(g, y) - 1); g->f = (u8)((g->f & FC) | FN | (v ? 0 : FZ) | ((v & 15) == 15 ? FH : 0)); gb_set(g, y, v); break; }
        case 6: gb_set(g, y, gb_f8(g)); break;
        default: {
            int a = g->r[7], c = (g->f >> 4) & 1;
            switch (y) {
            case 0: c = a >> 7; a = (a << 1) | c; g->f = (u8)(c ? FC : 0); break;
            case 1: c = a & 1; a = (a >> 1) | (c << 7); g->f = (u8)(c ? FC : 0); break;
            case 2: { int nc = a >> 7; a = (a << 1) | c; g->f = (u8)(nc ? FC : 0); break; }
            case 3: { int nc = a & 1; a = (a >> 1) | (c << 7); g->f = (u8)(nc ? FC : 0); break; }
            case 4:
                if (!(g->f & FN)) { if ((g->f & FC) || a > 0x99) { a += 0x60; c = 1; } if ((g->f & FH) || (a & 15) > 9) a += 6; }
                else { if (g->f & FC) a -= 0x60; if (g->f & FH) a -= 6; }
                a &= 255; g->f = (u8)((g->f & FN) | (a ? 0 : FZ) | (c ? FC : 0)); break;
            case 5: a ^= 255; g->f |= FN | FH; break;
            case 6: g->f = (u8)((g->f & FZ) | FC); break;
            default: g->f = (u8)((g->f & FZ) | (c ? 0 : FC)); break;
            }
            g->r[7] = (u8)a; break; }
        }
        return m * 4;
    }
    switch (z) {                                                                                   /* x == 3 */
    case 0:
        if (y < 4) { if ((y == 0 && !(g->f & FZ)) || (y == 1 && (g->f & FZ)) || (y == 2 && !(g->f & FC)) || (y == 3 && (g->f & FC))) { g->pc = gb_pop(g); m += 3; } }
        else if (y == 4) gb_wr(g, (u16)(0xFF00 + gb_f8(g)), g->r[7]);
        else if (y == 5) { int d = (signed char)gb_f8(g); g->f = (u8)(((g->sp & 15) + (d & 15)) > 15 ? FH : 0); if (((g->sp & 255) + (d & 255)) > 255) g->f |= FC; g->sp = (u16)(g->sp + d); }
        else if (y == 6) g->r[7] = gb_rd(g, (u16)(0xFF00 + gb_f8(g)));
        else { int d = (signed char)gb_f8(g); g->f = (u8)(((g->sp & 15) + (d & 15)) > 15 ? FH : 0); if (((g->sp & 255) + (d & 255)) > 255) g->f |= FC; gb_setpair(g, 2, (u16)(g->sp + d)); }
        break;
    case 1:
        if (!(y & 1)) { u16 v = gb_pop(g); int p = y >> 1; if (p == 3) { g->r[7] = (u8)(v >> 8); g->f = (u8)(v & 0xF0); } else gb_setpair(g, p, v); }
        else if (y == 1) g->pc = gb_pop(g);
        else if (y == 3) { g->pc = gb_pop(g); g->ime = 1; }
        else if (y == 5) g->pc = gb_pair(g, 2);
        else g->sp = gb_pair(g, 2);
        break;
    case 2:
        if (y < 4) { u16 a = gb_f16(g); if ((y == 0 && !(g->f & FZ)) || (y == 1 && (g->f & FZ)) || (y == 2 && !(g->f & FC)) || (y == 3 && (g->f & FC))) { g->pc = a; m++; } }
        else if (y == 4) gb_wr(g, (u16)(0xFF00 + g->r[1]), g->r[7]);
        else if (y == 5) gb_wr(g, gb_f16(g), g->r[7]);
        else if (y == 6) g->r[7] = gb_rd(g, (u16)(0xFF00 + g->r[1]));
        else g->r[7] = gb_rd(g, gb_f16(g));
        break;
    case 3:
        if (y == 0) g->pc = gb_f16(g);
        else if (y == 1) return gb_cb(g) * 4;
        else if (y == 6) g->ime = 0;
        else if (y == 7) g->ei = 1;
        break;
    case 4:
        if (y < 4) { u16 a = gb_f16(g); if ((y == 0 && !(g->f & FZ)) || (y == 1 && (g->f & FZ)) || (y == 2 && !(g->f & FC)) || (y == 3 && (g->f & FC))) { gb_push(g, g->pc); g->pc = a; m += 3; } }
        break;
    case 5:
        if (!(y & 1)) { int p = y >> 1; gb_push(g, p == 3 ? (u16)((g->r[7] << 8) | g->f) : gb_pair(g, p)); }
        else if (y == 1) { u16 a = gb_f16(g); gb_push(g, g->pc); g->pc = a; }
        break;
    case 6: gb_alu(g, y, gb_f8(g)); break;
    default: gb_push(g, g->pc); g->pc = (u16)(y * 8); break;
    }
    return m * 4;
}
static void gb_step(GB *g) {
    int enable = g->ei, cyc; g->ei = 0;
    u8 pend = (u8)(g->io[0x0F] & g->io[0x7F] & 0x1F);
    if (pend) g->halt = 0;
    if (g->ime && pend) {
        int b = 0; while (!((pend >> b) & 1)) b++;
        g->ime = 0; g->io[0x0F] &= (u8)~(1 << b); gb_push(g, g->pc); g->pc = (u16)(0x40 + 8 * b); cyc = 20;
    } else if (g->halt) cyc = 4;
    else cyc = gb_exec(g);
    if (enable) g->ime = 1;
    gb_time(g, cyc);
}
/* one full frame (until the next vertical blank); draw = render scanlines through gb_emit() */
static void gb_frame(GB *g, int draw) { g->draw = (u8)draw; g->fdone = 0; while (!g->fdone) gb_step(g); }

/* ---------------- setup ---------------- */
static void gb_zero(void *p, u32 n) { u8 *b = (u8 *)p; for (u32 i = 0; i < n; i++) b[i] = 0; }
/* rom: whole cartridge image (kept by the core, caller frees). Returns a new core or 0 (bad ROM / no memory). */
static GB *gb_new(u8 *rom, u32 size) {
    if (size < 0x8000 || size > (8u << 20)) return 0;
    GB *g = (GB *)MALLOC(sizeof(GB)); if (!g) return 0;
    gb_zero(g, sizeof *g);
    g->rom = rom; g->romsz = size;
    { int nb = 2; while ((u32)nb * 0x4000 < size) nb <<= 1; g->nbanks = nb; }
    u8 t = rom[0x147]; g->cgb = (rom[0x143] & 0x80) ? 1 : 0;
    g->mbc = (t >= 1 && t <= 3) ? 1 : (t == 5 || t == 6) ? 2 : (t >= 0x0F && t <= 0x13) ? 3 : (t >= 0x19 && t <= 0x1E) ? 5 : 0;
    g->bat = (t == 3 || t == 6 || t == 9 || t == 0x0F || t == 0x10 || t == 0x13 || t == 0x1B || t == 0x1E);
    { static const u32 rs[6] = { 0, 2048, 8192, 32768, 131072, 65536 }; u8 c = rom[0x149]; g->ersz = c < 6 ? rs[c] : 0; if (g->mbc == 2) g->ersz = 512; }
    if (g->ersz) { g->eram = (u8 *)MALLOC(g->ersz); if (!g->eram) { FREE(g); return 0; } for (u32 i = 0; i < g->ersz; i++) g->eram[i] = 0; }
    g->b1 = 1; g->wbk = 1;
    g->r[7] = g->cgb ? 0x11 : 0x01; g->f = g->cgb ? 0x80 : 0xB0; g->sp = 0xFFFE; g->pc = 0x100;
    if (g->cgb) { g->r[0] = 0; g->r[1] = 0; g->r[2] = 0xFF; g->r[3] = 0x56; g->r[4] = 0; g->r[5] = 0x0D; }
    else { g->r[0] = 0; g->r[1] = 0x13; g->r[2] = 0; g->r[3] = 0xD8; g->r[4] = 0x01; g->r[5] = 0x4D; }
    g->io[0x40] = 0x91; g->io[0x47] = 0xFC; g->io[0x48] = 0xFF; g->io[0x49] = 0xFF; g->io[0x0F] = 0x01; g->io[0x7F] = 0; g->io[0x41] = 0; g->mode = 1;
    for (int i = 0; i < 32; i++) { g->cbg[i] = 0xFFFF; g->cob[i] = 0xFFFF; }
    for (int i = 0; i < 64; i++) { g->bgp[i] = 0xFF; g->obp[i] = 0xFF; }
    gb_banks(g);
    return g;
}
