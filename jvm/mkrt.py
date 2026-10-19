"""mkrt.py: generate the Java runtime sources (gen/*.py -> rt/), compile them with the JDK8 javac (-source 1.3 -target 1.1) and pack the classes into rt.pak
(a plain ZIP; the watch reads it with the same zip code as the games' jars).   python mkrt.py [-o rt.pak]"""
import os, subprocess, sys, zipfile, glob, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
JAVAC = r"C:\Program Files\Eclipse Adoptium\jdk-8.0.504.1-hotspot\bin\javac.exe"
def main():
    out = os.path.join(HERE, "rt.pak")
    if "-o" in sys.argv: out = sys.argv[sys.argv.index("-o") + 1]
    gen = os.path.join(HERE, "gen")
    shutil.rmtree(os.path.join(HERE, "rt"), ignore_errors=True)
    for g in sorted(glob.glob(os.path.join(gen, "rt_*.py"))):
        r = subprocess.run([sys.executable, "-X", "utf8", g], capture_output=True, text=True)
        if r.returncode: sys.exit("generator failed: %s\n%s%s" % (g, r.stdout, r.stderr))
    srcs = [p for p in glob.glob(os.path.join(HERE, "rt", "**", "*.java"), recursive=True)]
    cls = os.path.join(HERE, "rt_classes"); shutil.rmtree(cls, ignore_errors=True); os.makedirs(cls)
    r = subprocess.run([JAVAC, "-nowarn", "-source", "1.3", "-target", "1.1", "-g:none", "-d", cls] + srcs, capture_output=True, text=True)
    if r.returncode: sys.exit("javac failed:\n" + r.stdout + r.stderr)
    n = 0; tot = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(glob.glob(os.path.join(cls, "**", "*.class"), recursive=True)):
            name = os.path.relpath(p, cls).replace("\\", "/"); data = open(p, "rb").read()
            z.writestr(name, data); n += 1; tot += len(data)
    print("%d classes, %d bytes of class files -> %s (%d bytes)" % (n, tot, out, os.path.getsize(out)))
main()
