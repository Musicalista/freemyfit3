# fit3-mods

Unofficial tools and code for the **Samsung Galaxy Fit3 (SM-R390), firmware R390XXU0AZA3**. Not affiliated with Samsung.
They inject small apps into the watch's main firmware image and repackage it for the fit3-flasher web installer
(a static page that sends the firmware to the watch over Web Serial).

> **Warning.** Flashing modified firmware can **brick your watch** and voids the warranty. Do it at your own risk, with a charged battery and the
> original `stock-aza3.bin` at hand. **This repository contains no Samsung firmware**, and you must not publish the packages you build
> (they contain Samsung code): build your own from **your** `stock-aza3.bin`.

## What is inside

| Feature | Where |
|---|---|
| **T9 reply keyboard.** Every quick reply on a notification opens a full-screen pastel keypad (multi-tap, accents, `ç`, UTF-8), pre-filled with that reply; the received message is shown above the text box. OK sends it through the normal reply path. | `kbd/`, `inject/fit3_apps.c` |
| **"Extra apps" launcher** (Settings > the relabelled "Tips and tutorials" entry): pastel 2x3 tile grid, two pages, swipe/arrows. UI text follows the watch language (pt-BR / English). | `inject/menu.inc.c` |
| **Mini games:** Snake, Flappy, Tetris, 2048. | `inject/minigames.inc.c` |
| **Internet over your phone's Bluetooth tethering** (BNEP / PAN): a small TCP/IP stack (ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP) and an encrypted channel (ChaCha20-Poly1305, pre-shared key) to a private proxy that fetches pages (HTTPS included) and answers in a tiny text format. | `net/`, `inject/net.inc.c` |
| **Text web reader** (numbered links, scrolling, history) and **Groq AI chat** (the API key never leaves the phone/PC). | `inject/webreader.inc.c`, `webbridge/groq.js`, `android/` |
| **Fit3 Hub (Android app):** runs the proxy on the phone and manages the watch over the flasher's Bluetooth serial service (AT commands, read files). It does **not** flash firmware. | `android/` |
| **PC bridge:** a headless Chrome rendered to the watch (16 colours) with PC keyboard/mouse control, plus the same proxy running on a PC. | `webbridge/` |
| A tiny JVM core and a software RGB565 3D rasteriser (experiments). | `src/` |

Third-party games (a Minecraft rd-132211 port, an FPS, a Doom-style game) are **not** part of this repository.

## How it works
1. `tools/fwd.py` unpacks the FWD package of `stock-aza3.bin`. The main image has its own CRC32 and no signature.
2. The apps are freestanding C (Thumb, no libc, **no writable globals**: they run from flash) compiled into the ~62 KB of zeros at the end of the image.
3. `inject/inject.py` retargets existing `bl` instructions to our code ("hooks", see `docs/FIRMWARE_MAP.md`).
4. `tools/repack.py` recomputes every CRC and `tools/val.mjs` runs the **flasher's own validator** on the result.

Notes on the Internet app: the Bluetooth stack runs in its own thread, so every L2CAP call is posted to it with the firmware's
`app_bt_call_func_in_bt_thread`; its callbacks have no user context, so they find our state by walking LVGL's timer list; received frames are queued
and consumed by the GUI-thread timer. The watch finds the proxy at the DHCP gateway (the phone itself).

## Requirements (Windows)
Python 3.10+ (`pip install capstone`), Node 18+, the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-*`), your own `stock-aza3.bin` and a copy of the flasher project (for `validation-worker.js`).

```
set ARM_GNU_BIN=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin
set FIT3_STOCK=C:\path\to\stock-aza3.bin
set FLASHER_DIR=C:\path\to\fit3-flasher
python inject/build2.py apps              # keyboard + launcher + mini games + web reader
python inject/build2.py net --net         # ... plus Internet + AI (generates webbridge/proxy.key and inject/net_cfg.h)
```
The package lands in `dist/` (the flasher's validator runs at the end). The first build also generates the font (`kbd/make_font.py`, uses a font from your Windows).
`--marker-only` restores the old behaviour (only the `...` quick reply opens the keyboard).

## The proxy
The watch talks to a proxy through the phone's Bluetooth tethering. Pick one:

* **Android app (recommended):** build `android/` (below), install it, paste your Groq key, tap *Start proxy*. The firmware default is `host=gw`: the watch connects to the tethering gateway, i.e. the phone.
* **PC:** `node webbridge/proxy.js 8788` (open the port in your firewall; the phone must be on the same network) and build the firmware with `host=<PC ip>` in `net/proxy.cfg`.

Both use the same 32-byte key, `webbridge/proxy.key` (created on the first `--net` build, **never committed**). Rebuild the firmware and the app together if you regenerate it.
Groq: put your key in `webbridge/groq.key` (PC) or in the app (phone). Optional `webbridge/groq.cfg` (see `groq.cfg.example`) sets the model.

## Fit3 Hub (Android)
```
set ANDROID_SDK_ROOT=C:\path\to\sdk      (platform android-34 and build-tools 34.x installed)
set JAVA_HOME=C:\path\to\jdk-17
python android/build.py                  -> android/dist/fit3-hub.apk (debug-signed)
```
No Gradle: `javac -> d8 -> aapt2 -> zipalign -> apksigner`. The watch actions use the same wire protocol as the flasher
(`00AT^NAME=arg1=arg2`; file read `061/062`). Only a small safe subset of the watch's factory AT commands is exposed.

## Tests you can run on a PC
* `kbd/kbd_test.c`: types a sentence with accents on the T9 pad. `inject/host_menu_test.c`, `host_hook_test.c`, `host_mg_test.c`, `host_2048_test.c`, `host_web_test.c`:
  the real app code with LVGL/BT faked (`gcc -DNO_GAMES -DMINI_GAMES -I kbd -I src -I net -DM3D_NO_BMP ...`).
* `net/chacha_test.c` (`node net/gen_vectors.js` makes vectors from Node's reference implementation) and `android/` `ChachaTest`.
* `net/phone_sim.py`: a simulated NAP phone (BNEP, DHCP, DNS, ARP, ICMP, a TCP server bridged to the real proxy, random packet loss) driving the real stack.
* `net/app_sim.py` (+ `ai`): the whole reader app against the simulated phone, the proxy (`FIT3_PROXY=java` runs the Android app's server instead of `proxy.js`),
  a local web server and a fake Groq server.

## Status
* **Verified on a real watch:** code injection, vibration, touch, LVGL image canvas, full-screen UI, and the reply keyboard (used for WhatsApp replies).
* **Verified only on a PC (simulation):** the launcher, mini games, T9 pad details, the whole network stack, the proxy, the Groq flow and the Android app's server.
* **Not verified on hardware:** the Bluetooth glue of the Internet app (L2CAP event codes beyond open/close, packet buffer layout, thread-safety, whether the phone accepts
  the PANU's MAC), the Android app's UI and its watch actions, and the firmware's `/user/web_*` file access.

## License
No license has been chosen yet, so by default all rights are reserved. Open an issue if you need a license for a specific use.
