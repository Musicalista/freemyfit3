"""ppm2png.py: convert PPM (P6) screenshots to PNG. usage: python ppm2png.py a.ppm [b.ppm ...]  (writes a.png next to each)"""
import sys, zlib, struct


def conv(f):
    d = open(f, 'rb').read(); parts = d.split(b'\n', 3); w, h = map(int, parts[1].split()); px = parts[3]
    raw = b''.join(b'\0' + px[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def ch(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    open(f[:-4] + '.png', 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))


for f in sys.argv[1:]: conv(f)
