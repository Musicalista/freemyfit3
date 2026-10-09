/* helpers shared by several apps (kept out of the game files so a no-games build still has them) */
static void mg_num(u16 *g, int x, int y, int v, int sc, uint16_t col) {
    char d[8]; int n = 0; if (v < 0) v = 0; do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 7);
    for (int i = 0; i < n; i++) put(g, x + i * 8 * sc, y, (uint32_t)(uint8_t)d[n - 1 - i], sc, col);
}
static int mg_numw(int v) { int n = 1; while (v >= 10) { v /= 10; n++; } return n; }
