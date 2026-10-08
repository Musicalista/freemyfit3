"""build2.py <name>: build the full 'apps' firmware (reply keyboard + 3D game + Settings 'Apps extras' menu + relabelled language packs).
Compiles apps_entry.s + fit3_apps.c into the free area of the AZA3 main image, retargets the hook `bl`s, patches language packs,
repacks the FWD package, validates it with the flasher's own validator and copies it to Downloads."""
import subprocess, sys, os, shutil, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inject import enc_bl, BASE, FREE_START, FREE_LEN
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); TOOLS = os.path.join(ROOT, "tools"); WORK = os.path.join(ROOT, "work"); DIST = os.path.join(ROOT, "dist")
A = os.environ.get("ARM_GNU_BIN", r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin").rstrip("\\") + "\\"
STOCK = os.environ.get("FIT3_STOCK", os.path.join(ROOT, "firmware", "stock-aza3.bin"))
H = os.path.dirname(os.path.abspath(__file__))

CAVE = 0x2c3a0524
HOOKS = [(0x2c1d0802, 'cave', 0x2c112f58),        # quick-reply click handler: send_reply -> keyboard / game
         (0x2c1b76a0, 'cave_menu', 0x2c2974f8)]   # settings tutorials page: log call -> schedules the Apps extras menu
LANG = [68, 52, 30, 13]
name = sys.argv[1] if len(sys.argv) > 1 else 'apps'
DEFS = ['-DGAME_CS'] if '--cs' in sys.argv else (['-DNO_3D'] if '--no3d' in sys.argv else [])
os.chdir(H)

def run(*a):
    r = subprocess.run(a, capture_output=True, text=True)
    if r.returncode: print(r.stdout + r.stderr); raise SystemExit('FAILED: ' + ' '.join(a[:2]))
    return r.stdout

if not os.path.exists(os.path.join(ROOT, 'kbd', 'kbd_font.h')): run(sys.executable, os.path.join(ROOT, 'kbd', 'make_font.py'))
run(A + 'arm-none-eabi-as.exe', '-mcpu=cortex-m4', '-mthumb', '-o', 'apps_entry.o', 'apps_entry.s')
run(*([A + 'arm-none-eabi-gcc.exe', '-mcpu=cortex-m4', '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-mthumb', '-Os', '-fno-math-errno', '-ffreestanding', '-fno-builtin',
    '-fno-tree-loop-distribute-patterns', '-fno-stack-protector', '-I', '../kbd', '-I', '../src', '-DM3D_NO_BMP'] + DEFS + ['-c', 'fit3_apps.c', '-o', 'apps.o']))
run(A + 'arm-none-eabi-ld.exe', '-Ttext=0x%x' % CAVE, '-e', 'cave', '-o', 'apps.elf', 'apps_entry.o', 'apps.o')
run(A + 'arm-none-eabi-objcopy.exe', '-O', 'binary', 'apps.elf', 'apps.bin')
for ln in run(A + 'arm-none-eabi-size.exe', '-A', 'apps.elf').splitlines():
    p = ln.split()
    if p and p[0] in ('.data', '.bss') and int(p[1]) != 0: raise SystemExit('FATAL: writable section: ' + ln + ' (flash cave is read-only)')
und = run(A + 'arm-none-eabi-nm.exe', '-u', 'apps.elf').strip()
if und: raise SystemExit('FATAL: undefined symbols: ' + und)
syms = {}
for ln in run(A + 'arm-none-eabi-nm.exe', 'apps.elf').splitlines():
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
blob = open('apps.bin', 'rb').read(); off = CAVE - BASE
assert syms['cave'] == CAVE
print('blob bytes: %d of %d free (%.1f%%)' % (len(blob), FREE_LEN, 100.0 * len(blob) / FREE_LEN))
assert off % 4 == 0 and FREE_START <= off and off + len(blob) <= FREE_START + FREE_LEN, 'blob does not fit the free area'

run(sys.executable, '-I', os.path.join(TOOLS, 'fwd.py'), STOCK, os.path.join(WORK, 'out')) if not os.path.isdir(os.path.join(WORK, 'out')) else None
import glob; MAIN = glob.glob(os.path.join(WORK, 'out', '*_k1_user__ota__app__*.bin'))[0]
img = bytearray(open(MAIN, 'rb').read())
assert all(b == 0 for b in img[off:off + len(blob)]), 'cave area not empty'
img[off:off + len(blob)] = blob
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
for hook, sym, orig in HOOKS:
    ins = list(md.disasm(bytes(img[hook - BASE:hook - BASE + 4]), hook))
    assert ins and ins[0].mnemonic == 'bl' and int(ins[0].op_str.lstrip('#'), 16) == orig, 'hook %#x is not bl %#x' % (hook, orig)
    img[hook - BASE:hook - BASE + 4] = enc_bl(hook, syms[sym])
    chk = list(md.disasm(bytes(img[hook - BASE:hook - BASE + 4]), hook))[0]
    assert int(chk.op_str.lstrip('#'), 16) == syms[sym]
    print('hook %#x: bl %#x -> bl %#x (%s)' % (hook, orig, syms[sym], sym))
open('main_%s.bin' % name, 'wb').write(img)

run(sys.executable, '-I', 'patch_lang.py')
out = os.path.join(H, 'fit3-%s.bin' % name)
args = [sys.executable, '-I', os.path.join(TOOLS, 'repack.py'), STOCK, out, '2=main_%s.bin' % name]
args += ['%d=lang\\%d.bin' % (i, i) for i in LANG if os.path.exists('lang\\%d.bin' % i)]
run(*args)
print(subprocess.run(['node', os.path.join(TOOLS, 'val.mjs'), out], capture_output=True, text=True).stdout.strip())
os.makedirs(DIST, exist_ok=True); shutil.copy(out, os.path.join(DIST, 'fit3-%s.bin' % name))
print('-> dist/fit3-%s.bin (main image size: %d bytes of 0x380000 validator cap)' % (name, len(img)))
