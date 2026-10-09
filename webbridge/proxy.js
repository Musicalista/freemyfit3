// Fit3 secure proxy: the watch (over Bluetooth tethering + our own TCP/IP stack) talks to this TCP server through an encrypted channel
// (ChaCha20-Poly1305 with a pre-shared key). It fetches pages (HTTPS included) and answers with the same text format as the PC bridge.
//
//   node proxy.js [port] [keyfile]       default port 8788, key from ./proxy.key (created on first run: 64 hex chars)
//
// Wire format (must match net/netstack.c):
//   client -> 8 random bytes (cn)          server -> 8 random bytes (sn), then an encrypted record "OK"
//   session key: k1 = block(psk, ctr 0, cn||0000)[0..32]; key = block(k1, ctr 1, sn||0000)[0..32]       (block = one ChaCha20 block)
//   record: u16 BE length of (ciphertext+16-byte tag), ciphertext, tag. AEAD nonce = dir(1 byte: 0 client->server, 1 server->client) 000 counter(8 bytes BE); AAD = the 2 length bytes
//   requests: 'G' + url or search text | 'A' + question for the Groq AI (needs groq.key) | 'E' + text (echo, for tests) | 'B' + decimal N (N test bytes)
//   responses: records of  flag(1 byte: 0 = more follows, 1 = last) + up to 1000 data bytes
'use strict';
const net = require('net'), crypto = require('crypto'), fs = require('fs'), path = require('path');
const PORT = parseInt(process.argv[2] || '8788', 10);
const KEYFILE = process.argv[3] || path.join(__dirname, 'proxy.key');
const CHUNK = 1000;

function loadKey() {
  if (!fs.existsSync(KEYFILE)) { fs.writeFileSync(KEYFILE, crypto.randomBytes(32).toString('hex') + '\n'); console.log('created key file', KEYFILE); }
  const k = Buffer.from(fs.readFileSync(KEYFILE, 'utf8').trim(), 'hex');
  if (k.length !== 32) throw new Error('key file must hold 64 hex characters');
  return k;
}
function block(key, counter, nonce12) {                              // one raw ChaCha20 block via the cipher's keystream
  const iv = Buffer.alloc(16); iv.writeUInt32LE(counter, 0); nonce12.copy(iv, 4);
  return crypto.createCipheriv('chacha20', key, iv).update(Buffer.alloc(64));
}
function deriveKey(psk, cn, sn) {
  const n1 = Buffer.alloc(12); cn.copy(n1, 0); const k1 = block(psk, 0, n1).subarray(0, 32);
  const n2 = Buffer.alloc(12); sn.copy(n2, 0); return block(k1, 1, n2).subarray(0, 32);
}
function nonceFor(dir, ctr) { const n = Buffer.alloc(12); n[0] = dir; n.writeBigUInt64BE(BigInt(ctr), 4); return n; }

function serve(psk, getPage, getAi) {
  return net.createServer((sock) => {
    let buf = Buffer.alloc(0), key = null, rctr = 0n, sctr = 0n, busy = false, dead = false; const session = { hist: [] };
    const send = (payload) => {
      const len = Buffer.alloc(2); len.writeUInt16BE(payload.length + 16);
      const c = crypto.createCipheriv('chacha20-poly1305', key, nonceFor(1, sctr++), { authTagLength: 16 }); c.setAAD(len, { plaintextLength: payload.length });
      sock.write(Buffer.concat([len, c.update(payload), c.final(), c.getAuthTag()]));
    };
    const reply = (data) => {                                          // chunked, flag byte first
      let off = 0; if (!data.length) return send(Buffer.from([1]));
      while (off < data.length) { const part = data.subarray(off, off + CHUNK); off += part.length; send(Buffer.concat([Buffer.from([off >= data.length ? 1 : 0]), part])); }
    };
    const handle = async (msg) => {
      const t = String.fromCharCode(msg[0]), arg = msg.subarray(1).toString('utf8');
      try {
        if (t === 'E') reply(Buffer.from(arg));
        else if (t === 'B') { const n = Math.min(parseInt(arg, 10) || 0, 65000); const b = Buffer.alloc(n); for (let i = 0; i < n; i++) b[i] = i % 251; reply(b); }
        else if (t === 'G') reply(await getPage(arg.trim()));
        else if (t === 'A') reply(getAi ? await getAi(arg.trim(), session) : Buffer.from('!IA indisponivel'));
        else reply(Buffer.from('?unknown request'));
      } catch (e) { reply(Buffer.from('!' + (e && e.message || e))); }
    };
    const pump = async () => {
      if (busy || dead) return; busy = true;
      try {
        for (;;) {
          if (!key) {
            if (buf.length < 8) break;
            const cn = buf.subarray(0, 8), sn = crypto.randomBytes(8); buf = buf.subarray(8);
            key = deriveKey(psk, Buffer.from(cn), sn); sock.write(sn); send(Buffer.from('OK')); continue;
          }
          if (buf.length < 2) break; const cl = buf.readUInt16BE(0); if (cl < 17 || cl > 1200) throw new Error('bad record length');
          if (buf.length < 2 + cl) break;
          const lenb = buf.subarray(0, 2), ct = buf.subarray(2, 2 + cl - 16), tag = buf.subarray(2 + cl - 16, 2 + cl); buf = buf.subarray(2 + cl);
          const d = crypto.createDecipheriv('chacha20-poly1305', key, nonceFor(0, rctr++), { authTagLength: 16 }); d.setAAD(lenb, { plaintextLength: ct.length }); d.setAuthTag(tag);
          const msg = Buffer.concat([d.update(ct), d.final()]); if (msg.length) await handle(msg);
        }
      } catch (e) { dead = true; sock.destroy(); } finally { busy = false; }
    };
    sock.on('data', (d) => { buf = Buffer.concat([buf, d]); pump(); }); sock.on('error', () => {});
  });
}
// keep a page well inside the watch's 24 KB buffer: at most 40 links, links cut to 120 chars
function shrink(buf) {
  const text = buf.toString('utf8'), i = text.indexOf('\n\u0001\n'); if (i < 0) return buf;
  const head = text.slice(0, i + 3), links = text.slice(i + 3).split('\n').filter(Boolean).slice(0, 40).map((l) => l.slice(0, 120));
  let out = head + links.join('\n') + '\n'; if (Buffer.byteLength(out) > 23500) out = out.slice(0, 23000) + '\n\u0001\n';
  return Buffer.from(out, 'utf8');
}
module.exports = { serve, loadKey, deriveKey, block, shrink };

if (require.main === module) {
  const { page } = require('./server.js'), { askGroq } = require('./groq.js');
  const psk = loadKey();
  const getPage = async (q) => { const b = shrink(await page(q, 1)); return b; };
  serve(psk, getPage, askGroq).listen(PORT, '0.0.0.0', () => console.log('Fit3 secure proxy listening on :' + PORT + '  (key: ' + KEYFILE + ')'));
}
