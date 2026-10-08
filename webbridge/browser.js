// Remote browser for the Fit3: drives a real headless Chrome through the DevTools Protocol and returns 256x340 16-colour frames.
// Frame file layout (binary, little endian), 192-byte header + payload:
//   0 'F','1' | 2 u32 seq | 6 u16 W | 8 u16 H | 10 u8 mode (0 raw 4bpp hi-nibble-first, 1 RLE) | 11 u8 zoom index | 12 u32 payload length
//   16 palette: 16 x u16 RGB565 | 48 title[40] | 88 url[104] | 192 payload
//   RLE: one byte per run: high nibble = run-1 (1..16 pixels), low nibble = colour index.
'use strict';
const { spawn } = require('child_process');
const fs = require('fs'), os = require('os'), path = require('path'), zlib = require('zlib');
const W = 256, H = 340, PORT = 9333, ZOOMS = [256, 320, 400, 512];
const CHROMES = [process.env.CHROME_PATH, 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe', 'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
  path.join(process.env.LOCALAPPDATA || '', 'Google\\Chrome\\Application\\chrome.exe'), 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe', 'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe'];
const MOBILE_UA = 'Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Mobile Safari/537.36';
const SUPPORTED = new Set([...'áàâãäéèêëíìîïóòôõöúùûüçñÁÀÂÃÄÉÈÊÍÌÓÒÔÕÖÚÙÛÜÇÑ¿¡ºª°€£']);
const delay = ms => new Promise(r => setTimeout(r, ms));
function safeText(s) {
  s = String(s || '').replace(/[“”„«»]/g, '"').replace(/[‘’‚`´]/g, "'").replace(/[–—−]/g, '-').replace(/…/g, '...');
  let o = ''; for (const ch of s) { const c = ch.codePointAt(0); if ((c >= 32 && c <= 126) || SUPPORTED.has(ch)) o += ch; else { const b = ch.normalize('NFD').replace(/[\u0300-\u036f]/g, ''); o += (/^[\x20-\x7e]+$/.test(b) ? b : '?'); } }
  return o;
}

// ---------- PNG decoder (8-bit RGB / RGBA, non-interlaced) ----------
function decodePng(buf) {
  let pos = 8, w = 0, h = 0, ct = 0, idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos), type = buf.toString('latin1', pos + 4, pos + 8), data = buf.subarray(pos + 8, pos + 8 + len); pos += 12 + len;
    if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); ct = data[9]; if (data[8] !== 8 || data[12] !== 0 || (ct !== 2 && ct !== 6)) throw new Error('png format ' + ct + '/' + data[8]); }
    else if (type === 'IDAT') idat.push(data); else if (type === 'IEND') break;
  }
  const bpp = ct === 6 ? 4 : 3, stride = w * bpp, raw = zlib.inflateSync(Buffer.concat(idat)), out = Buffer.alloc(w * h * 4); let prev = Buffer.alloc(stride);
  for (let y = 0; y < h; y++) {
    const f = raw[y * (stride + 1)], line = Buffer.from(raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1)));
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? line[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
      if (f === 1) line[i] = (line[i] + a) & 255; else if (f === 2) line[i] = (line[i] + b) & 255; else if (f === 3) line[i] = (line[i] + ((a + b) >> 1)) & 255;
      else if (f === 4) { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c); line[i] = (line[i] + (pa <= pb && pa <= pc ? a : pb <= pc ? b : c)) & 255; }
    }
    for (let x = 0; x < w; x++) { const o = (y * w + x) * 4; out[o] = line[x * bpp]; out[o + 1] = line[x * bpp + 1]; out[o + 2] = line[x * bpp + 2]; out[o + 3] = bpp === 4 ? line[x * bpp + 3] : 255; }
    prev = line;
  }
  return { w, h, data: out };
}

// ---------- 16-colour median-cut quantiser ----------
function quantize(img) {
  const n = img.w * img.h, hist = new Uint32Array(65536), keys = new Uint16Array(n);
  for (let i = 0; i < n; i++) { const k = ((img.data[i * 4] >> 3) << 11) | ((img.data[i * 4 + 1] >> 2) << 5) | (img.data[i * 4 + 2] >> 3); keys[i] = k; hist[k]++; }
  let colors = []; for (let k = 0; k < 65536; k++) if (hist[k]) colors.push(k);
  const rgb = k => [((k >> 11) & 31) << 3 | 4, ((k >> 5) & 63) << 2 | 2, (k & 31) << 3 | 4];
  let boxes = [colors];
  while (boxes.length < 16) {
    let bi = -1, best = -1, ch = 0;
    boxes.forEach((b, i) => { if (b.length < 2) return; let mn = [255, 255, 255], mx = [0, 0, 0]; for (const k of b) { const c = rgb(k); for (let j = 0; j < 3; j++) { if (c[j] < mn[j]) mn[j] = c[j]; if (c[j] > mx[j]) mx[j] = c[j]; } }
      const r = [mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]], j = r.indexOf(Math.max(...r)), score = r[j] * Math.sqrt(b.reduce((s, k) => s + hist[k], 0)); if (score > best) { best = score; bi = i; ch = j; } });
    if (bi < 0) break;
    const b = boxes[bi].sort((p, q) => rgb(p)[ch] - rgb(q)[ch]), tot = b.reduce((s, k) => s + hist[k], 0); let acc = 0, cut = 1;
    for (let i = 0; i < b.length; i++) { acc += hist[b[i]]; if (acc >= tot / 2) { cut = Math.max(1, Math.min(b.length - 1, i + 1)); break; } }
    boxes.splice(bi, 1, b.slice(0, cut), b.slice(cut));
  }
  const pal = boxes.map(b => { let r = 0, g = 0, bl = 0, t = 0; for (const k of b) { const c = rgb(k), w = hist[k]; r += c[0] * w; g += c[1] * w; bl += c[2] * w; t += w; } return [Math.round(r / t), Math.round(g / t), Math.round(bl / t)]; });
  while (pal.length < 16) pal.push([0, 0, 0]);
  const map = new Int16Array(65536).fill(-1), idx = new Uint8Array(n);
  for (let i = 0; i < n; i++) {
    const k = keys[i]; let m = map[k];
    if (m < 0) { const c = rgb(k); let bd = 1e9; m = 0; for (let p = 0; p < pal.length; p++) { const d = (c[0] - pal[p][0]) ** 2 + (c[1] - pal[p][1]) ** 2 + (c[2] - pal[p][2]) ** 2; if (d < bd) { bd = d; m = p; } } map[k] = m; }
    idx[i] = m;
  }
  return { pal: pal.map(c => ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)), idx };
}
function encode(idx) {
  const raw = Buffer.alloc((idx.length + 1) >> 1); for (let i = 0; i < idx.length; i += 2) raw[i >> 1] = (idx[i] << 4) | (i + 1 < idx.length ? idx[i + 1] : 0);
  const rle = []; for (let i = 0; i < idx.length;) { let r = 1; while (r < 16 && i + r < idx.length && idx[i + r] === idx[i]) r++; rle.push(((r - 1) << 4) | idx[i]); i += r; }
  return rle.length < raw.length ? { mode: 1, data: Buffer.from(rle) } : { mode: 0, data: raw };
}
function buildFrame(seq, zoom, title, url, q) {
  const e = encode(q.idx), head = Buffer.alloc(192);
  head.write('F1', 0, 'latin1'); head.writeUInt32LE(seq >>> 0, 2); head.writeUInt16LE(W, 6); head.writeUInt16LE(H, 8); head[10] = e.mode; head[11] = zoom; head.writeUInt32LE(e.data.length, 12);
  q.pal.forEach((p, i) => head.writeUInt16LE(p, 16 + i * 2)); head.write(safeText(title).slice(0, 39), 48, 'latin1'); head.write(safeText(url).slice(0, 103), 88, 'latin1');
  return Buffer.concat([head, e.data]);
}

// ---------- Chrome DevTools Protocol ----------
class Browser {
  constructor(opts) { this.port = (opts && opts.port) || PORT; this.ws = null; this.id = 0; this.pend = new Map(); this.waiters = []; this.zoom = 1; this.ready = null; this.loading = false; }
  async start() {
    if (this.ready) return this.ready;
    return this.ready = (async () => {
      const exe = CHROMES.find(p => p && fs.existsSync(p)); if (!exe) throw new Error('Chrome/Edge nao encontrado (defina CHROME_PATH)');
      const dir = path.join(os.tmpdir(), 'fit3-chrome-profile-' + this.port);
      this.proc = spawn(exe, ['--headless=new', '--remote-debugging-port=' + this.port, '--user-data-dir=' + dir, '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--hide-scrollbars', '--mute-audio', '--disable-extensions', 'about:blank'], { stdio: 'ignore' });
      this.proc.on('exit', () => { this.ready = null; this.ws = null; });
      let target = null;
      for (let i = 0; i < 60 && !target; i++) { try { const l = await (await fetch('http://127.0.0.1:' + this.port + '/json/list')).json(); target = l.find(t => t.type === 'page'); } catch {} if (!target) await delay(250); }
      if (!target) throw new Error('Chrome nao iniciou');
      this.ws = new WebSocket(target.webSocketDebuggerUrl); await new Promise((res, rej) => { this.ws.onopen = res; this.ws.onerror = () => rej(new Error('falha no WebSocket')); });
      this.ws.onmessage = ev => { const m = JSON.parse(ev.data); if (m.id && this.pend.has(m.id)) { const p = this.pend.get(m.id); this.pend.delete(m.id); m.error ? p.rej(new Error(m.error.message)) : p.res(m.result); } else if (m.method) { if (m.method === 'Page.frameStartedLoading') this.loading = true; this.waiters = this.waiters.filter(w => { if (w.method === m.method) { w.res(m.params); return false; } return true; }); } };
      await this.send('Page.enable'); await this.send('Emulation.setUserAgentOverride', { userAgent: MOBILE_UA }); await this.metrics();
    })();
  }
  send(method, params = {}) { const id = ++this.id; return new Promise((res, rej) => { this.pend.set(id, { res, rej }); this.ws.send(JSON.stringify({ id, method, params })); setTimeout(() => { if (this.pend.has(id)) { this.pend.delete(id); rej(new Error('timeout ' + method)); } }, 20000); }); }
  once(method, ms) { return new Promise(res => { const w = { method, res }; this.waiters.push(w); setTimeout(() => { this.waiters = this.waiters.filter(x => x !== w); res(null); }, ms); }); }
  get cssW() { return ZOOMS[this.zoom]; } get scale() { return W / this.cssW; }
  async metrics() { await this.send('Emulation.setDeviceMetricsOverride', { width: this.cssW, height: Math.round(H / this.scale), deviceScaleFactor: this.scale, mobile: true }); }
  async nav(url) { const l = this.once('Page.loadEventFired', 12000); this.loading = true; await this.send('Page.navigate', { url }); await l; await delay(500); this.loading = false; }
  async mouse(type, x, y, extra = {}) { await this.send('Input.dispatchMouseEvent', Object.assign({ type, x, y, button: 'left', buttons: type === 'mousePressed' ? 1 : 0, clickCount: 1, pointerType: 'mouse' }, extra)); }
  async run(line) {
    const sp = line.indexOf(' '), cmd = (sp < 0 ? line : line.slice(0, sp)).toLowerCase(), arg = sp < 0 ? '' : line.slice(sp + 1);
    if (cmd === 'go' || cmd === 'nav') {
      const q = arg.trim(); const isUrl = /^https?:\/\//i.test(q) || (/^[\w-]+(\.[\w-]+)+(:\d+)?(\/\S*)?$/.test(q) && !/\s/.test(q));
      await this.nav(isUrl ? (/^https?:\/\//i.test(q) ? q : 'https://' + q) : 'https://duckduckgo.com/?q=' + encodeURIComponent(q));
    } else if (cmd === 'click') {
      const [x, y] = arg.split(/\s+/).map(Number); const cx = x / this.scale, cy = y / this.scale; this.loading = false;
      await this.mouse('mouseMoved', cx, cy, { button: 'none', clickCount: 0 }); await this.mouse('mousePressed', cx, cy); await this.mouse('mouseReleased', cx, cy);
      await delay(700); if (this.loading) await this.once('Page.loadEventFired', 8000); await delay(300);
    } else if (cmd === 'scroll') {
      const dy = Number(arg) / this.scale; await this.mouse('mouseWheel', this.cssW / 2, 100, { button: 'none', clickCount: 0, deltaX: 0, deltaY: dy }); await delay(350);
    } else if (cmd === 'type') { await this.send('Input.insertText', { text: arg }); await delay(150); }
    else if (cmd === 'key') {
      const K = { Backspace: [8, 'Backspace'], Tab: [9, 'Tab'], Enter: [13, 'Enter'], Escape: [27, 'Escape'], PageUp: [33, 'PageUp'], PageDown: [34, 'PageDown'], End: [35, 'End'], Home: [36, 'Home'],
        ArrowLeft: [37, 'ArrowLeft'], ArrowUp: [38, 'ArrowUp'], ArrowRight: [39, 'ArrowRight'], ArrowDown: [40, 'ArrowDown'], Delete: [46, 'Delete'] };
      const k = K[arg.trim()]; if (k) { for (const t of ['keyDown', 'keyUp']) await this.send('Input.dispatchKeyEvent', { type: t, key: k[1], code: k[1], windowsVirtualKeyCode: k[0], nativeVirtualKeyCode: k[0], text: (t === 'keyDown' && k[1] === 'Enter') ? '\r' : undefined });
        await delay(k[1] === 'Enter' ? 700 : 150); if (k[1] === 'Enter' && this.loading) await this.once('Page.loadEventFired', 8000); }
    }
    else if (cmd === 'enter') {
      for (const t of ['keyDown', 'keyUp']) await this.send('Input.dispatchKeyEvent', { type: t, key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13, nativeVirtualKeyCode: 13, text: t === 'keyDown' ? '\r' : undefined });
      await delay(700); if (this.loading) await this.once('Page.loadEventFired', 8000); await delay(300);
    } else if (cmd === 'back' || cmd === 'fwd') {
      const h = await this.send('Page.getNavigationHistory'), i = h.currentIndex + (cmd === 'back' ? -1 : 1);
      if (i >= 0 && i < h.entries.length) { const l = this.once('Page.loadEventFired', 10000); await this.send('Page.navigateToHistoryEntry', { entryId: h.entries[i].id }); await l; await delay(400); }
    } else if (cmd === 'reload') { const l = this.once('Page.loadEventFired', 12000); await this.send('Page.reload'); await l; await delay(400); }
    else if (cmd === 'zoom') { this.zoom = Math.max(0, Math.min(ZOOMS.length - 1, this.zoom + (Number(arg) || 0))); await this.metrics(); await delay(400); }
  }
  async frame(seq, lines) {
    await this.start();
    for (const l of lines) { try { await this.run(l.trim()); } catch (e) { console.log('cmd error', l, e.message); } }
    const r = await this.send('Page.captureScreenshot', { format: 'png' }); const img = decodePng(Buffer.from(r.data, 'base64'));
    if (img.w !== W || img.h !== H) { /* nearest-neighbour fit to the fixed frame size */
      const d = Buffer.alloc(W * H * 4); for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const sx = Math.min(img.w - 1, Math.floor(x * img.w / W)), sy = Math.min(img.h - 1, Math.floor(y * img.h / H)); img.data.copy(d, (y * W + x) * 4, (sy * img.w + sx) * 4, (sy * img.w + sx) * 4 + 4); }
      img.w = W; img.h = H; img.data = d;
    }
    let title = '', url = ''; try { title = (await this.send('Runtime.evaluate', { expression: 'document.title', returnByValue: true })).result.value; url = (await this.send('Runtime.evaluate', { expression: 'location.href', returnByValue: true })).result.value; } catch {}
    return buildFrame(seq, this.zoom, title, url, quantize(img));
  }
  async close() { try { this.ws && this.ws.close(); this.proc && this.proc.kill(); } catch {} }
}
module.exports = { Browser, decodePng, quantize, buildFrame, W, H };
