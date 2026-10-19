"""appcc.py: compile a C file into a Fit3 app (.f3a) that the watch can load from /user/apps/.

   python appcc.py src/hello.c --id hello --pt "Ola" --en "Hello" [--bg RRGGBB --fg RRGGBB] [-o dist/hello.f3a]

The app is compiled for Cortex-M4 (Thumb, hard float) as freestanding C, linked at address 0 with apps/app.ld; the absolute words (R_ARM_ABS32) are listed in a relocation
table that the watch applies at load time (base address of the RAM block). Layout of the .f3a file (little endian):
   0 magic "F3A1" | 4 u16 api version, u16 flags | 8 u32 image size | 12 u32 bss size | 16 u32 entry offset | 20 u32 CRC32(image) | 24 u32 relocation count | 28 reserved
   32 name (Portuguese, 16 bytes) | 48 name (English, 16 bytes) | 64 image | then relocation offsets (u32 each)
Environment: ARM_GNU_BIN (default: Arm GNU Toolchain 14.2 in Program Files (x86))."""
import argparse, os, re, struct, subprocess, sys, tempfile, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.environ.get("ARM_GNU_BIN", r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin")
EXE = ".exe" if os.name == "nt" else ""
def tool(n): return os.path.join(BIN, "arm-none-eabi-" + n + EXE)
def run(*a):
    r = subprocess.run(a, capture_output=True, text=True)
    if r.returncode: sys.exit("FAILED: %s\n%s%s" % (" ".join(a[:3]), r.stdout, r.stderr))
    return r.stdout

def build(src, app_id, name_pt, name_en, out):
    tmp = tempfile.mkdtemp(prefix="f3a_"); obj = os.path.join(tmp, "app.o"); elf = os.path.join(tmp, "app.elf"); binf = os.path.join(tmp, "app.bin")
    run(tool("gcc"), "-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb", "-Os", "-ffreestanding", "-fno-builtin", "-fno-tree-loop-distribute-patterns", "-fno-stack-protector",
        "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1", "-mword-relocations", "-fno-pic", "-fno-exceptions", "-ffunction-sections", "-fdata-sections", "-I", HERE, "-c", src, "-o", obj)
    run(tool("ld"), "-q", "--gc-sections", "-T", os.path.join(HERE, "app.ld"), "-o", elf, obj)
    und = run(tool("nm"), "-u", elf).strip()
    if und: sys.exit("undefined symbols (no libc on the watch: avoid memset/memcpy/division helpers; use F3Api): " + " ".join(und.split()))
    syms = {}
    for ln in run(tool("nm"), elf).splitlines():
        p = ln.split()
        if len(p) == 3: syms[p[2]] = int(p[0], 16)
    if "f3app_main" not in syms: sys.exit("the app must define f3app_main()")
    img_end = syms["__image_end"]; bss_end = syms["__bss_end"]; entry = syms["f3app_main"] & ~1
    run(tool("objcopy"), "-O", "binary", "--only-section=.text", "--only-section=.data", elf, binf)
    img = open(binf, "rb").read()
    if len(img) < img_end: img += b"\0" * (img_end - len(img))
    img = img[:img_end]
    relocs = []; section = None
    for ln in run(tool("objdump"), "-r", elf).splitlines():
        m = re.match(r"RELOCATION RECORDS FOR \[(.*)\]", ln)
        if m: section = m.group(1); continue
        m = re.match(r"([0-9a-fA-F]+)\s+(R_ARM_\w+)\s+(\S+)", ln)
        if not m or section not in (".text", ".data"): continue
        off, typ = int(m.group(1), 16), m.group(2)
        if typ in ("R_ARM_ABS32", "R_ARM_TARGET1"): relocs.append(off)
        elif typ in ("R_ARM_THM_CALL", "R_ARM_THM_JUMP24", "R_ARM_THM_JUMP11", "R_ARM_THM_JUMP8", "R_ARM_THM_PC8", "R_ARM_THM_PC12", "R_ARM_REL32", "R_ARM_THM_JUMP19", "R_ARM_V4BX"): pass
        else: sys.exit("unsupported relocation %s at 0x%x in %s (call only functions of this file or of F3Api; avoid libc/libgcc helpers)" % (typ, off, section))
    for r in relocs:
        if r % 4 or r + 4 > len(img): sys.exit("misaligned or out-of-range relocation at 0x%x" % r)
    if len(img) > 40000: sys.exit("app too big (%d bytes, max 40000)" % len(img))
    name = lambda s: s.encode("latin-1", "replace")[:15].ljust(16, b"\0")
    hdr = struct.pack("<4sHHIIIIII", b"F3A1", 1, 0, len(img), bss_end - img_end, entry, zlib.crc32(img) & 0xffffffff, len(relocs), 0) + name(name_pt) + name(name_en)
    assert len(hdr) == 64
    data = hdr + img + b"".join(struct.pack("<I", r) for r in sorted(relocs))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True); open(out, "wb").write(data)
    return len(img), bss_end - img_end, len(relocs), len(data)

if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("src"); ap.add_argument("--id", required=True); ap.add_argument("--pt", required=True); ap.add_argument("--en", required=True)
    ap.add_argument("--bg", default="B0D4F8"); ap.add_argument("--fg", default="305AAC"); ap.add_argument("-o")
    a = ap.parse_args(); out = a.o or os.path.join(HERE, "dist", a.id + ".f3a")
    n = build(a.src, a.id, a.pt, a.en, out)
    meta = os.path.splitext(out)[0] + ".meta"; open(meta, "w", encoding="utf-8").write("%s;%s;%s;%s;%s\n" % (a.id, a.bg.upper(), a.fg.upper(), a.pt, a.en))
    print("%s: image %d B, bss %d B, %d relocations, file %d B" % (out, n[0], n[1], n[2], n[3]))
