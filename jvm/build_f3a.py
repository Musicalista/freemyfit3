"""build_f3a.py: build the J2ME player as a Fit3 app (j2me.f3a) from several C files (+ libgcc for 64-bit/double helpers).
   python build_f3a.py [-o dist/j2me.f3a] [--limit N]    (reuses the packaging code of apps/appcc.py)"""
import os, re, struct, subprocess, sys, tempfile, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
APPS = os.path.join(os.path.dirname(HERE), "apps")
sys.path.insert(0, APPS)
import appcc
SRCS = ["j2me_app.c", "midp.c", "vm.c", "jar.c", "inflate.c", "png.c", "sys.c"]

def build(out, limit):
    tmp = tempfile.mkdtemp(prefix="j2me_"); objs = []
    flags = ["-DF3APP", "-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb", "-Os", "-ffreestanding", "-fno-builtin", "-fno-tree-loop-distribute-patterns",
             "-fno-stack-protector", "-finput-charset=UTF-8", "-mword-relocations", "-fno-pic", "-fno-exceptions", "-ffunction-sections", "-fdata-sections",
             "-I", HERE, "-I", APPS, "-I", os.path.join(os.path.dirname(HERE), "kbd")]
    for s in SRCS:
        o = os.path.join(tmp, s.replace(".c", ".o")); appcc.run(appcc.tool("gcc"), *flags, "-c", os.path.join(HERE, s), "-o", o); objs.append(o)
    libgcc = appcc.run(appcc.tool("gcc"), "-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb", "-print-libgcc-file-name").strip()
    elf = os.path.join(tmp, "app.elf"); binf = os.path.join(tmp, "app.bin")
    appcc.run(appcc.tool("ld"), "-q", "--gc-sections", "-T", os.path.join(APPS, "app.ld"), "-o", elf, "-e", "f3app_main", *objs, libgcc)
    und = appcc.run(appcc.tool("nm"), "-u", elf).strip()
    if und: sys.exit("undefined symbols: " + " ".join(und.split()))
    syms = {}
    for ln in appcc.run(appcc.tool("nm"), elf).splitlines():
        p = ln.split()
        if len(p) == 3: syms[p[2]] = int(p[0], 16)
    img_end = syms["__image_end"]; bss_end = syms["__bss_end"]; entry = syms["f3app_main"] & ~1
    appcc.run(appcc.tool("objcopy"), "-O", "binary", "--only-section=.text", "--only-section=.data", elf, binf)
    img = open(binf, "rb").read()
    if len(img) < img_end: img += b"\0" * (img_end - len(img))
    img = img[:img_end]
    relocs = []; section = None
    for ln in appcc.run(appcc.tool("objdump"), "-r", elf).splitlines():
        m = re.match(r"RELOCATION RECORDS FOR \[(.*)\]", ln)
        if m: section = m.group(1); continue
        m = re.match(r"([0-9a-fA-F]+)\s+(R_ARM_\w+)\s+(\S+)", ln)
        if not m or section not in (".text", ".data"): continue
        off, typ = int(m.group(1), 16), m.group(2)
        if typ in ("R_ARM_ABS32", "R_ARM_TARGET1"): relocs.append(off)
        elif typ in ("R_ARM_THM_CALL", "R_ARM_THM_JUMP24", "R_ARM_THM_JUMP11", "R_ARM_THM_JUMP8", "R_ARM_THM_PC8", "R_ARM_THM_PC12", "R_ARM_REL32", "R_ARM_THM_JUMP19", "R_ARM_V4BX"): pass
        else: sys.exit("unsupported relocation %s at 0x%x in %s" % (typ, off, section))
    for r in relocs:
        if r % 4 or r + 4 > len(img): sys.exit("misaligned or out-of-range relocation at 0x%x" % r)
    print("image %d bytes, bss %d bytes, %d relocations (limit %d)" % (len(img), bss_end - img_end, len(relocs), limit))
    if len(img) > limit: sys.exit("app too big (%d > %d)" % (len(img), limit))
    name = lambda s: s.encode("latin-1", "replace")[:15].ljust(16, b"\0")
    hdr = struct.pack("<4sHHIIIIII", b"F3A1", 1, 0, len(img), bss_end - img_end, entry, zlib.crc32(img) & 0xffffffff, len(relocs), 0) + name("J2ME") + name("J2ME")
    data = hdr + img + b"".join(struct.pack("<I", r) for r in sorted(relocs))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True); open(out, "wb").write(data)
    print("wrote", out, len(data), "bytes")

if __name__ == "__main__":
    out = os.path.join(HERE, "dist", "j2me.f3a"); limit = 40000
    if "-o" in sys.argv: out = sys.argv[sys.argv.index("-o") + 1]
    if "--limit" in sys.argv: limit = int(sys.argv[sys.argv.index("--limit") + 1])
    build(out, limit)
