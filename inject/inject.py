"""inject.py: place a position-fixed blob into the free tail area of the AZA3 main image and redirect one `bl`.
usage: inject.py <main_in.bin> <main_out.bin> <blob.bin> <cave_addr> <hook_addr>
  cave_addr: absolute address (0x2c......) where the blob will live; hook_addr: address of an existing `bl` to retarget to cave_addr."""
import struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

BASE = 0x2c080000
FREE_START, FREE_LEN = 0x320522, 0xf3fe        # verified all-zero in stock AZA3

def enc_bl(site, target):
    off = target - (site + 4)
    assert -(1 << 24) <= off < (1 << 24) and off % 2 == 0
    s = (off >> 24) & 1; i1 = (off >> 23) & 1; i2 = (off >> 22) & 1
    j1 = (~(i1 ^ s)) & 1; j2 = (~(i2 ^ s)) & 1
    hw1 = 0xF000 | (s << 10) | ((off >> 12) & 0x3ff)
    hw2 = 0xD000 | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7ff)
    return struct.pack('<HH', hw1, hw2)

def main(a):
    src, dst, blob_f, cave, hook = a[0], a[1], a[2], int(a[3], 0), int(a[4], 0)
    img = bytearray(open(src, 'rb').read()); blob = open(blob_f, 'rb').read()
    off = cave - BASE
    assert off % 4 == 0 and FREE_START <= off and off + len(blob) <= FREE_START + FREE_LEN, 'cave outside free area'
    assert all(b == 0 for b in img[off:off + len(blob)]), 'cave area not empty'
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    ins = list(md.disasm(bytes(img[hook - BASE:hook - BASE + 4]), hook))
    assert ins and ins[0].mnemonic == 'bl', 'hook site is not a bl'
    orig = int(ins[0].op_str.lstrip('#'), 16)
    img[off:off + len(blob)] = blob
    img[hook - BASE:hook - BASE + 4] = enc_bl(hook, cave)
    chk = list(md.disasm(bytes(img[hook - BASE:hook - BASE + 4]), hook))[0]
    assert chk.mnemonic == 'bl' and int(chk.op_str.lstrip('#'), 16) == cave, chk
    open(dst, 'wb').write(img)
    print('hook %#x: bl %#x -> bl %#x ; blob %d bytes at %#x' % (hook, orig, cave, len(blob), cave))

if __name__ == '__main__':
    main(sys.argv[1:])
