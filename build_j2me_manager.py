#!/usr/bin/env python3
"""build_j2me_manager.sh: compila o app J2ME Manager (.f3a) para o Galaxy Fit3.
Uso: python build_j2me_manager.sh [--out dist/j2me_manager.f3a]
Requer: ARM GNU Toolchain (arm-none-eabi-gcc) em ARM_GNU_BIN ou C:\Program Files (x86)\...
"""
import os, sys, subprocess, tempfile, struct, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
APPS = os.path.join(HERE, "apps")
BIN = os.environ.get("ARM_GNU_BIN", r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin")
EXE = ".exe" if os.name == "nt" else ""
def tool(n): return os.path.join(BIN, "arm-none-eabi-" + n + EXE)
def run(*a):
    r = subprocess.run(a, capture_output=True, text=True)
    if r.returncode:
        print("FAILED:", " ".join(a[:3])); print(r.stdout); print(r.stderr); sys.exit(1)
    return r.stdout

def build(src, out):
    tmp = tempfile.mkdtemp(prefix="j2me_mgr_")
    obj = os.path.join(tmp, "app.o"); elf = os.path.join(tmp, "app.elf"); binf = os.path.join(tmp, "app.bin")
    run(tool("gcc"), "-DF3APP", "-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb", "-Os",
        "-ffreestanding", "-fno-builtin", "-fno-tree-loop-distribute-patterns", "-fno-stack-protector",
        "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1", "-mword-relocations", "-fno-pic", "-fno-exceptions",
        "-ffunction-sections", "-fdata-sections", "-I", APPS, "-I", HERE, "-c", src, "-o", obj)
    run(tool("ld"), "-q", "--gc-sections", "-T", os.path.join(APPS, "app.ld"), "-o", elf, obj, "-e", "f3app_main")
    und = run(tool("nm"), "-u", elf).strip()
    if und: sys.exit("undefined symbols: " + und)
    syms = {}
    for ln in run(tool("nm"), elf).splitlines():
        p = ln.split()
        if len(p) == 3: syms[p[2]] = int(p[0], 16)
    img_end = syms["__image_end"]; bss_end = syms["__bss_end"]; entry = syms["f3app_main"] & ~1
    run(tool("objcopy"), "-O", "binary", "--only-section=.text", "--only-section=.data", elf, binf)
    img = open(binf, "rb").read()
    if len(img) < img_end: img += b"\0" * (img_end - len(img))
    img = img[:img_end]
    relocs = []; section = None
    for ln in run(tool("objdump"), "-r", elf).splitlines():
        m = __import__("re").match(r"RELOCATION RECORDS FOR \[(.*)\]", ln)
        if m: section = m.group(1); continue
        m = __import__("re").match(r"([0-9a-fA-F]+)\s+(R_ARM_\w+)\s+(\S+)", ln)
        if not m or section not in (".text", ".data"): continue
        off, typ = int(m.group(1), 16), m.group(2)
        if typ in ("R_ARM_ABS32", "R_ARM_TARGET1"): relocs.append(off)
        elif typ in ("R_ARM_THM_CALL", "R_ARM_THM_JUMP24", "R_ARM_THM_JUMP11", "R_ARM_THM_JUMP8", "R_ARM_THM_PC8", "R_ARM_THM_PC12", "R_ARM_REL32", "R_ARM_THM_JUMP19", "R_ARM_V4BX"): pass
        else: sys.exit("unsupported relocation %s at 0x%x in %s" % (typ, off, section))
    for r in relocs:
        if r % 4 or r + 4 > len(img): sys.exit("misaligned relocation at 0x%x" % r)
    print("image %d bytes, bss %d bytes, %d relocations" % (len(img), bss_end - img_end, len(relocs)))
    if len(img) > 40000: sys.exit("app too big (%d > 40000)" % len(img))
    name = lambda s: s.encode("latin-1", "replace")[:15].ljust(16, b"\0")
    hdr = struct.pack("<4sHHIIIIII", b"F3A1", 1, 0, len(img), bss_end - img_end, entry, zlib.crc32(img) & 0xffffffff, len(relocs), 0) + name("J2ME Mgr") + name("J2ME Manager")
    data = hdr + img + b"".join(struct.pack("<I", r) for r in sorted(relocs))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    open(out, "wb").write(data)
    print("wrote", out, len(data), "bytes")

if __name__ == "__main__":
    out = os.path.join(HERE, "apps", "dist", "j2me_manager.f3a")
    if "--out" in sys.argv: out = sys.argv[sys.argv.index("--out") + 1]
    build(os.path.join(HERE, "apps", "src", "j2me_manager.c"), out)
