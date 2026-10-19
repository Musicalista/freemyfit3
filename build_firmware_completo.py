"""build_firmware_completo.py: monta um firmware completo J2ME + Game Boy para o Galaxy Fit3.
Empacota o player J2ME (j2me.f3a), o runtime Java (rt.pak), jogos J2ME (.jar) e
referencia firmwares .bin que ja incluem o emulador Game Boy.

Uso:
    py build_firmware_completo.py [--out firmware_fit3_tudo_em_um.zip] [--j2me-f3a caminho] [--rt-pak caminho]
"""
import argparse, os, shutil, struct, sys, tempfile, zipfile, zlib, json

HERE = os.path.dirname(os.path.abspath(__file__))
JVM = os.path.join(HERE, "jvm")
APPS = os.path.join(HERE, "apps")
WEBFLASHER_J2ME = os.path.join(HERE, "webflasher", "j2me", "j2me.f3a")
JVM_DIST_J2ME = os.path.join(JVM, "dist", "j2me.f3a")
JVM_RT_PAK = os.path.join(JVM, "rt.pak")
RELEASE = r"C:\Users\Usuario\Downloads\fit3-release-v1.8"
RELEASE_J2ME = os.path.join(RELEASE, "j2me")
RELEASE_BIN = RELEASE

def pick(*paths):
    for p in paths:
        if p and os.path.isfile(p):
            return p
    return None

def build_j2me_f3a(out):
    """Tenta recompilar o player J2ME com build_f3a.py (precisa do ARM toolchain)."""
    build_f3a = os.path.join(JVM, "build_f3a.py")
    if not os.path.isfile(build_f3a):
        return None
    import subprocess
    r = subprocess.run([sys.executable, build_f3a, "-o", out], capture_output=True, text=True, cwd=JVM)
    if r.returncode != 0:
        return None
    return out if os.path.isfile(out) else None

def build_rt_pak(out):
    """Tenta regenerar o runtime Java com mkrt.py (precisa do JDK 8 + javac)."""
    mkrt = os.path.join(JVM, "mkrt.py")
    if not os.path.isfile(mkrt):
        return None
    import subprocess
    r = subprocess.run([sys.executable, mkrt, "-o", out], capture_output=True, text=True, cwd=JVM)
    if r.returncode != 0:
        return None
    return out if os.path.isfile(out) else None

def collect_jars():
    """Coleta jogos J2ME (.jar) disponiveis no projeto."""
    jars = []
    test_dir = os.path.join(JVM, "test")
    if os.path.isdir(test_dir):
        for f in sorted(os.listdir(test_dir)):
            if f.lower().endswith(".jar"):
                jars.append(os.path.join(test_dir, f))
    # tambem procura na pasta apps (futuro)
    return jars

def f3a_info(path):
    data = open(path, "rb").read()
    if len(data) < 64 or data[:4] != b"F3A1":
        return None
    magic, api, flags, img_size, bss_size, entry, crc, nrel, reserved = struct.unpack_from("<4sHHIIIIII", data, 0)
    name_pt = data[32:48].split(b"\0")[0].decode("latin-1", "replace")
    name_en = data[48:64].split(b"\0")[0].decode("latin-1", "replace")
    return {"magic": magic.decode(), "api": api, "flags": flags, "img_size": img_size,
            "bss_size": bss_size, "entry": hex(entry), "crc": hex(crc), "nrel": nrel,
            "name_pt": name_pt, "name_en": name_en, "size": len(data)}

def main():
    ap = argparse.ArgumentParser(description="Monta firmware J2ME completo para Galaxy Fit3")
    ap.add_argument("--out", default=os.path.join(HERE, "firmware_fit3_j2me_completo.zip"))
    ap.add_argument("--j2me-f3a", default=None, help="caminho para j2me.f3a (player)")
    ap.add_argument("--rt-pak", default=None, help="caminho para rt.pak (runtime Java)")
    ap.add_argument("--rebuild", action="store_true", help="forca recompilar player e regenerar runtime")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)

    tmp = tempfile.mkdtemp(prefix="fit3_firmware_")
    try:
        # --- 1. Player J2ME (j2me.f3a) ---
        j2me_f3a = a.j2me_f3a or pick(WEBFLASHER_J2ME, JVM_DIST_J2ME, os.path.join(RELEASE_J2ME, "j2me.f3a"))
        if a.rebuild or not j2me_f3a:
            rebuilt = build_j2me_f3a(os.path.join(tmp, "j2me_rebuilt.f3a"))
            if rebuilt:
                j2me_f3a = rebuilt
        if not j2me_f3a:
            sys.exit("ERRO: nao foi possivel obter j2me.f3a. Forneca --j2me-f3a ou instale o ARM toolchain.")
        info = f3a_info(j2me_f3a)
        print("[*] Player J2ME: %s (%d bytes)" % (j2me_f3a, os.path.getsize(j2me_f3a)))
        if info:
            print("    nome_pt=%s nome_en=%s entry=%s crc=%s" % (info["name_pt"], info["name_en"], info["entry"], info["crc"]))
        shutil.copy(j2me_f3a, os.path.join(tmp, "j2me.f3a"))

        # --- 2. Runtime Java (rt.pak) ---
        rt_pak = a.rt_pak or pick(JVM_RT_PAK, os.path.join(RELEASE_J2ME, "rt.pak"))
        if a.rebuild or not rt_pak:
            rebuilt = build_rt_pak(os.path.join(tmp, "rt_rebuilt.pak"))
            if rebuilt:
                rt_pak = rebuilt
        if not rt_pak:
            sys.exit("ERRO: nao foi possivel obter rt.pak. Forneca --rt-pak ou configure o JDK 8.")
        print("[*] Runtime Java: %s (%d bytes)" % (rt_pak, os.path.getsize(rt_pak)))
        shutil.copy(rt_pak, os.path.join(tmp, "rt.pak"))

        # --- 3. Jogos J2ME (.jar) ---
        jars = collect_jars()
        jogos_dir = os.path.join(tmp, "jogos")
        os.makedirs(jogos_dir)
        for j in jars:
            shutil.copy(j, jogos_dir)
        print("[*] Jogos J2ME: %d" % len(jars))

        # --- 4. Firmwares .bin prontos que ja incluem Game Boy ---
        bin_ready = {}
        for name in ("fit3-max-cs.bin", "fit3-max-mc.bin", "fit3-net-gb.bin", "stock-aza3.bin"):
            p = os.path.join(RELEASE_BIN, name)
            if os.path.isfile(p):
                bin_ready[name] = p
        bins_dir = os.path.join(tmp, "firmware_bin")
        os.makedirs(bins_dir, exist_ok=True)
        for name, p in bin_ready.items():
            shutil.copy(p, os.path.join(bins_dir, name))
        print("[*] Firmwares .bin com Game Boy: %d" % len(bin_ready))

        # --- 5. Teclado (resposta rápida) -- binarios ja compilados ---
        kbd_dir = os.path.join(tmp, "teclado")
        os.makedirs(kbd_dir, exist_ok=True)
        kbd_bins = []
        for name in ("fit3-kbd-only.bin", "main_kbd-only.bin", "fit3-kbd-reply.bin", "main_kbd-reply.bin"):
            p = os.path.join(HERE, "inject", name)
            if os.path.isfile(p):
                shutil.copy(p, os.path.join(kbd_dir, name)); kbd_bins.append(name)
        print("[*] Teclado: %d binarios" % len(kbd_bins))

        # --- 6. Manifesto do firmware ---
        manifest = {
            "version": "1.0",
            "device": "Galaxy Fit3 (SM-R390)",
            "chip": "best1502x",
            "components": {
                "player_j2me": "j2me.f3a",
                "runtime_java": "rt.pak",
                "jogos_j2me": [os.path.basename(j) for j in jars],
                "gameboy": {
                    "disponivel_em": "firmware_bin/*.bin",
                    "rom": "envie seu .gb ou .gbc para /user/gb.gb",
                    "save": "progresso salvo em /user/gb.sav",
                    "menu": "Apps extras > GameBoy",
                },
                "teclado": {
                    "disponivel_em": "teclado/*.bin",
                    "descricao": "Teclado na resposta rápida (quick reply). Flashe fit3-kbd-only.bin ou main_kbd-only.bin.",
                    "binarios": kbd_bins,
                },
            },
            "player_info": info,
            "firmwares_bin": {n: os.path.getsize(p) for n, p in bin_ready.items()},
            "instrucoes_j2me": "Instale pelo app Fit3 Manager (Android), aba 'Apps e jogos': toque em 'Instalar um jogo J2ME (.jar)'. O app envia o player, o runtime e o jogo automaticamente.",
            "instrucoes_gameboy": "Flashe um dos .bin em firmware_bin (ex.: fit3-net-gb.bin) que ja inclui o emulador Game Boy. No relogio: Apps extras > GameBoy. Envie sua ROM (.gb ou .gbc) para /user/gb.gb. Use um jogo seu.",
            "instrucoes_teclado": "Flashe teclado/fit3-kbd-only.bin (ou main_kbd-only.bin). No relogio, na resposta rápida, toque em \"...\" para abrir o teclado.",
            "limites_j2me": "ate 8 jogos, jar de ate ~1,5 MB. Precisa de firmware com 'Meus apps' (store-base, max-*, net-*).",
            "limites_gameboy": "Sem som. CPU, MBC1/2/3/5, DMG + CGB video. Sem ROMs incluídas — traga o seu proprio jogo.",
        }
        import json
        with open(os.path.join(tmp, "firmware.json"), "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)

        # --- 7. Pacote ZIP final (tudo em um) ---
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
            for root, dirs, files in os.walk(tmp):
                for fn in files:
                    full = os.path.join(root, fn)
                    arc = os.path.relpath(full, tmp)
                    z.write(full, arc)
        print("[+] Firmware TUDO EM UM gravado em: %s" % out)
        print("    tamanho: %d bytes" % os.path.getsize(out))
        print("    conteudo: j2me.f3a, rt.pak, jogos/*.jar, firmware_bin/*.bin, teclado/*.bin, firmware.json")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

if __name__ == "__main__":
    main()
