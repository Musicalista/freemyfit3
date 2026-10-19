// Generates ChaCha20-Poly1305 test vectors with Node's reference implementation: key nonce aad pt out(ct+tag), all hex.
const c = require('crypto'); const lines = [];
const sizes = [0, 1, 15, 16, 17, 63, 64, 65, 127, 128, 129, 255, 500, 1000, 1400];
for (const n of sizes) for (const a of [0, 5, 16, 33]) {
  const key = c.randomBytes(32), nonce = c.randomBytes(12), aad = c.randomBytes(a), pt = c.randomBytes(n);
  const ci = c.createCipheriv('chacha20-poly1305', key, nonce, { authTagLength: 16 }); ci.setAAD(aad, { plaintextLength: n });
  const out = Buffer.concat([ci.update(pt), ci.final(), ci.getAuthTag()]);
  lines.push([key, nonce, aad, pt, out].map(b => b.length ? b.toString('hex') : '-').join(' '));
}
require('fs').writeFileSync('vectors.txt', lines.join('\n') + '\n'); console.log(lines.length, 'vectors');
