# raw2png.py in.raw out.png [w h]: converts a raw little-endian RGB565 frame to PNG
import sys, zlib, struct
w = int(sys.argv[3]) if len(sys.argv) > 3 else 240
h = int(sys.argv[4]) if len(sys.argv) > 4 else 320
d = open(sys.argv[1], 'rb').read()
rows = []
for y in range(h):
    row = bytearray()
    for x in range(w):
        c = d[(y * w + x) * 2] | (d[(y * w + x) * 2 + 1] << 8)
        row += bytes(((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3))
    rows.append(b'\0' + bytes(row))
def ch(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
open(sys.argv[2], 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(b''.join(rows), 6)) + ch(b'IEND', b''))
