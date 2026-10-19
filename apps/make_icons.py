"""make_icons.py: draws the 32x32 one-bit icons of the apps (1 = ink, drawn in the tile's background colour over its foreground colour).
Each icon is 128 bytes: 4 bytes per row, most significant bit on the left. ICONS maps an app id to its 256 hex characters.
    python make_icons.py sheet.ppm      writes a preview sheet (all icons, 8 per row, ink = light on dark)"""
import sys

N = 32
FONT = {  # 3x5 digits and a few signs; each row is 3 bits
    '0': (7, 5, 5, 5, 7), '1': (2, 6, 2, 2, 7), '2': (7, 1, 7, 4, 7), '3': (7, 1, 7, 1, 7), '4': (5, 5, 7, 1, 1), '5': (7, 4, 7, 1, 7),
    '6': (7, 4, 7, 5, 7), '7': (7, 1, 2, 2, 2), '8': (7, 5, 7, 5, 7), '9': (7, 5, 7, 1, 7), '+': (0, 2, 7, 2, 0), '?': (7, 1, 3, 0, 2),
}

class Img:
    def __init__(self): self.p = [[0] * N for _ in range(N)]
    def set(self, x, y, v=1):
        if 0 <= x < N and 0 <= y < N: self.p[y][x] = v
    def rect(self, x, y, w, h, v=1):
        for j in range(h):
            for i in range(w): self.set(x + i, y + j, v)
    def disc(self, cx, cy, r, v=1):
        for j in range(-r, r + 1):
            for i in range(-r, r + 1):
                if i * i + j * j <= r * r + r // 2: self.set(cx + i, cy + j, v)
    def ring(self, cx, cy, r, t, v=1):
        for j in range(-r, r + 1):
            for i in range(-r, r + 1):
                d = i * i + j * j
                if (r - t) * (r - t) < d <= r * r + r // 2: self.set(cx + i, cy + j, v)
    def line(self, x0, y0, x1, y1, w=2, v=1):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for k in range(n + 1):
            x = x0 + (x1 - x0) * k // n; y = y0 + (y1 - y0) * k // n
            self.rect(x - w // 2, y - w // 2, w, w, v)
    def poly(self, pts, v=1):
        for y in range(N):
            xs = []
            for k in range(len(pts)):
                (xa, ya), (xb, yb) = pts[k], pts[(k + 1) % len(pts)]
                if (ya <= y < yb) or (yb <= y < ya): xs.append(xa + (y - ya) * (xb - xa) / float(yb - ya))
            xs.sort()
            for a in range(0, len(xs) - 1, 2):
                for x in range(int(round(xs[a])), int(round(xs[a + 1]))): self.set(x, y, v)
    def text(self, ch, x, y, s, v=1):
        for r, bits in enumerate(FONT[ch]):
            for c in range(3):
                if bits & (4 >> c): self.rect(x + c * s, y + r * s, s, s, v)
    def hex(self):
        out = ''
        for row in self.p:
            for b in range(4):
                byte = 0
                for k in range(8): byte = (byte << 1) | row[b * 8 + k]
                out += '%02X' % byte
        return out

def snake(m):
    for a, b in (((24, 6), (8, 6)), ((8, 6), (8, 16)), ((8, 16), (24, 16)), ((24, 16), (24, 26)), ((24, 26), (10, 26))): m.line(a[0], a[1], b[0], b[1], 5)
    m.disc(8, 26, 4); m.disc(7, 25, 1, 0)
def flappy(m):
    m.disc(15, 16, 10); m.disc(19, 13, 3, 0); m.disc(20, 13, 1); m.poly([(24, 14), (31, 17), (24, 20)]); m.disc(10, 18, 5, 0); m.disc(10, 18, 3)
def tetris(m):
    m.rect(3, 3, 26, 8); m.rect(12, 11, 8, 8); m.rect(3, 21, 12, 8); m.rect(17, 21, 12, 8); m.rect(15, 3, 1, 8, 0); m.rect(15, 21, 2, 8, 0); m.rect(3, 19, 26, 2, 0)
def g2048(m):
    m.rect(2, 2, 28, 28); m.rect(2, 2, 2, 2, 0); m.rect(28, 2, 2, 2, 0); m.rect(2, 28, 2, 2, 0); m.rect(28, 28, 2, 2, 0); m.text('2', 10, 7, 4, 0)
def pong(m):
    m.rect(3, 7, 4, 18); m.rect(25, 7, 4, 18); m.disc(16, 16, 3)
    for y in range(2, 30, 6): m.rect(15, y, 2, 3)
def velha(m):
    m.rect(11, 2, 3, 28); m.rect(18, 2, 3, 28); m.rect(2, 11, 28, 3); m.rect(2, 18, 28, 3)
    m.line(3, 3, 9, 9, 2); m.line(9, 3, 3, 9, 2); m.ring(25, 25, 4, 2)
def memoria(m):
    m.rect(2, 2, 13, 13); m.rect(17, 2, 13, 13); m.rect(2, 17, 13, 13); m.rect(17, 17, 13, 13); m.disc(8, 8, 3, 0); m.rect(21, 21, 5, 5, 0); m.disc(23, 8, 2, 0)
def sudoku(m):
    m.rect(2, 2, 28, 28); m.rect(4, 4, 24, 24, 0)
    for k in (11, 19): m.rect(k, 4, 2, 24); m.rect(4, k, 24, 2)
    m.rect(14, 14, 4, 4); m.rect(5, 5, 4, 4); m.rect(23, 23, 4, 4)
def campomin(m):
    m.disc(16, 16, 8)
    for a, b in ((16, 3), (16, 29), (3, 16), (29, 16), (7, 7), (25, 25), (7, 25), (25, 7)):
        m.line(16, 16, a, b, 2)
    m.disc(13, 13, 2, 0)
def breakout(m):
    for r in range(3):
        for c in range(3): m.rect(2 + c * 10, 3 + r * 7, 9, 5)
    m.disc(19, 21, 2); m.rect(9, 27, 14, 3)
def simon(m):
    m.disc(16, 16, 14); m.rect(14, 0, 4, 32, 0); m.rect(0, 14, 32, 4, 0); m.disc(16, 16, 3)
def reflexo(m):
    m.poly([(19, 1), (6, 18), (14, 18), (11, 31), (26, 13), (17, 13)])
def relogio(m):
    m.ring(16, 16, 14, 3); m.line(16, 16, 16, 7, 2); m.line(16, 16, 22, 19, 2); m.disc(16, 16, 2)
def alarme(m):
    m.disc(16, 14, 9); m.rect(7, 14, 18, 10); m.rect(4, 23, 24, 3); m.disc(16, 28, 2); m.rect(15, 2, 2, 4)
def timer(m):
    m.ring(16, 18, 12, 3); m.rect(13, 1, 6, 4); m.line(16, 18, 16, 10, 2); m.line(16, 18, 21, 21, 2); m.line(25, 8, 28, 5, 2)
def contador(m):
    m.rect(3, 13, 11, 4); m.rect(6, 10, 5, 10); m.text('1', 18, 7, 4)
def sorteio(m):
    m.ring(16, 16, 14, 3); m.disc(16, 16, 7); m.rect(15, 8, 2, 16, 0); m.rect(8, 15, 16, 2, 0)
def respirar(m):
    m.ring(16, 16, 14, 2); m.ring(16, 16, 9, 2); m.disc(16, 16, 4)
def lanterna(m):
    m.rect(2, 12, 15, 9); m.poly([(17, 9), (23, 6), (23, 27), (17, 24)]); m.rect(5, 15, 6, 3, 0)
    m.line(26, 16, 31, 16, 2); m.line(26, 9, 30, 5, 2); m.line(26, 23, 30, 27, 2)
def hello(m):
    m.rect(3, 4, 26, 17); m.poly([(8, 21), (15, 21), (8, 28)]); m.disc(10, 12, 2, 0); m.disc(16, 12, 2, 0); m.disc(22, 12, 2, 0)
def paint(m):
    m.disc(16, 16, 14); m.disc(10, 11, 3, 0); m.disc(17, 7, 3, 0); m.disc(24, 12, 3, 0); m.disc(24, 21, 3, 0); m.disc(11, 23, 4, 0)
def dice(m):
    m.rect(3, 3, 26, 26); m.rect(3, 3, 1, 1, 0); m.rect(28, 3, 1, 1, 0); m.rect(3, 28, 1, 1, 0); m.rect(28, 28, 1, 1, 0)
    for x, y in ((10, 10), (22, 10), (16, 16), (10, 22), (22, 22)): m.disc(x, y, 2, 0)

DRAW = {'snake': snake, 'flappy': flappy, 'tetris': tetris, 'g2048': g2048, 'pong': pong, 'velha': velha, 'memoria': memoria, 'sudoku': sudoku, 'campomin': campomin,
        'breakout': breakout, 'simon': simon, 'reflexo': reflexo, 'relogio': relogio, 'alarme': alarme, 'timer': timer, 'contador': contador, 'sorteio': sorteio,
        'respirar': respirar, 'lanterna': lanterna, 'hello': hello, 'paint': paint, 'dice': dice}

def build():
    out = {}
    for k, f in DRAW.items():
        m = Img(); f(m); out[k] = m
    return out
IMGS = build()
ICONS = {k: m.hex() for k, m in IMGS.items()}

if __name__ == '__main__' and len(sys.argv) > 1:
    ids = list(DRAW); cols = 8; sc = 4; pad = 6; rows = (len(ids) + cols - 1) // cols
    W = cols * (N * sc + pad) + pad; H = rows * (N * sc + pad) + pad; buf = bytearray(b'\x1c\x20\x3e' * (W * H))
    for n, k in enumerate(ids):
        ox = pad + (n % cols) * (N * sc + pad); oy = pad + (n // cols) * (N * sc + pad)
        for y in range(N * sc):
            for x in range(N * sc):
                v = IMGS[k].p[y // sc][x // sc]; c = (240, 240, 250) if v else (52, 84, 168)
                i = ((oy + y) * W + ox + x) * 3; buf[i:i + 3] = bytes(c)
    open(sys.argv[1], 'wb').write(b'P6\n%d %d\n255\n' % (W, H) + bytes(buf))
