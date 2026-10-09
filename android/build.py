"""build.py: compile the Fit3 Hub app into an APK without Gradle (javac -> d8 -> aapt2 -> zipalign -> apksigner).

Environment: ANDROID_SDK_ROOT (or ANDROID_HOME) with a platform (android-34) and build-tools installed, and JAVA_HOME pointing to JDK 17.
The shared key is read from ../webbridge/proxy.key (created if missing) and baked into the APK as an asset; it must match the firmware build.
Output: android/dist/fit3-hub.apk, signed with a local debug key (android/fit3hub-debug.keystore, never committed)."""
import os, secrets, shutil, subprocess, sys, zipfile

H = os.path.dirname(os.path.abspath(__file__))
SDK = os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME") or sys.exit("set ANDROID_SDK_ROOT to your Android SDK")
JAVA = os.environ.get("JAVA_HOME") or sys.exit("set JAVA_HOME to a JDK 17")
NT = os.name == "nt"; EXE = ".exe" if NT else ""; BAT = ".bat" if NT else ""
OUT = os.path.join(H, "out"); DIST = os.path.join(H, "dist")

def newest(path, prefix):
    names = sorted(n for n in os.listdir(path) if n.startswith(prefix)); return os.path.join(path, names[-1]) if names else sys.exit("not found in " + path)
BT = newest(os.path.join(SDK, "build-tools"), ""); AJAR = os.path.join(newest(os.path.join(SDK, "platforms"), "android-"), "android.jar")

def run(*a, **k):
    r = subprocess.run(a, capture_output=True, text=True, **k)
    if r.returncode: print(r.stdout + r.stderr); raise SystemExit("FAILED: " + " ".join(str(x) for x in a[:3]))
    return r.stdout

env = dict(os.environ, JAVA_HOME=JAVA)
shutil.rmtree(OUT, ignore_errors=True); os.makedirs(os.path.join(OUT, "cls")); os.makedirs(os.path.join(OUT, "assets")); os.makedirs(DIST, exist_ok=True)
keyf = os.path.join(H, "..", "webbridge", "proxy.key")
if not os.path.exists(keyf): open(keyf, "w").write(secrets.token_hex(32) + "\n"); print("created", keyf)
key = open(keyf).read().strip(); assert len(key) == 64, "proxy.key must be 64 hex characters"
open(os.path.join(OUT, "assets", "proxy.key"), "w").write(key + "\n")

src = os.path.join(H, "src", "com", "fit3", "hub")
srcs = [os.path.join(src, f) for f in os.listdir(src) if f.endswith(".java") and f not in ("ChachaTest.java", "TestServer.java")]   # the PC test helpers are not part of the app
run(os.path.join(JAVA, "bin", "javac" + EXE), "-encoding", "UTF-8", "--release", "8", "-classpath", AJAR, "-d", os.path.join(OUT, "cls"), *srcs)
classes = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(OUT, "cls")) for f in fs if f.endswith(".class")]
run(os.path.join(BT, "d8" + BAT), "--release", "--min-api", "24", "--lib", AJAR, "--output", OUT, *classes, env=env)
run(os.path.join(BT, "aapt2" + EXE), "link", "-o", os.path.join(OUT, "base.apk"), "-I", AJAR, "--manifest", os.path.join(H, "AndroidManifest.xml"),
    "-A", os.path.join(OUT, "assets"), "--min-sdk-version", "24", "--target-sdk-version", "33")
with zipfile.ZipFile(os.path.join(OUT, "base.apk"), "a") as z: z.write(os.path.join(OUT, "classes.dex"), "classes.dex")
run(os.path.join(BT, "zipalign" + EXE), "-f", "4", os.path.join(OUT, "base.apk"), os.path.join(OUT, "aligned.apk"))
ks = os.path.join(H, "fit3hub-debug.keystore"); pw = secrets.token_hex(8) if not os.path.exists(ks + ".pw") else open(ks + ".pw").read().strip()
if not os.path.exists(ks):
    open(ks + ".pw", "w").write(pw)
    run(os.path.join(JAVA, "bin", "keytool" + EXE), "-genkeypair", "-keystore", ks, "-storepass", pw, "-keypass", pw, "-alias", "fit3hub", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000", "-dname", "CN=Fit3 Hub")
apk = os.path.join(DIST, "fit3-hub.apk")
run(os.path.join(BT, "apksigner" + BAT), "sign", "--ks", ks, "--ks-pass", "pass:" + pw, "--key-pass", "pass:" + pw, "--out", apk, os.path.join(OUT, "aligned.apk"), env=env)
run(os.path.join(BT, "apksigner" + BAT), "verify", apk, env=env)
print("APK: %s (%d bytes)" % (apk, os.path.getsize(apk)))
