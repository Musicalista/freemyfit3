// Fit3 web bridge (PC side). Run:  node server.js   then open http://127.0.0.1:8787/ in Chrome/Edge (Web Serial needs a secure context; localhost qualifies).
// Serves bridge.html and two JSON endpoints that turn a web page / a search into the tiny text format the watch reader understands.
'use strict';
const http = require('http'), fs = require('fs'), path = require('path');
const { Browser, quantize, buildFrame, W: FW, H: FH } = require('./browser.js');
let remote = null;
const PORT = 8787, MAX_BODY = 18000, MAX_LINKS = 60;
const SUPPORTED = new Set([...'áàâãäéèêëíìîïóòôõöúùûüçñÁÀÂÃÄÉÈÊÍÌÓÒÔÕÖÚÙÛÜÇÑ¿¡ºª°€£']);

// ---- text normalisation: the watch font has ASCII + Portuguese accents + a few symbols; map everything else to something readable
function normalize(s) {
  s = s.replace(/[​-‏⁠﻿­]/g, '').replace(/[  -   　]/g, ' ');
  s = s.replace(/[“”„«»]/g, '"').replace(/[‘’‚‹›`´]/g, "'").replace(/[–—−]/g, '-').replace(/…/g, '...').replace(/[•·●▪■◦]/g, '*').replace(/[×✕✖]/g, 'x').replace(/→/g, '->').replace(/←/g, '<-').replace(/™/g, '(tm)').replace(/©/g, '(c)').replace(/®/g, '(R)');
  let out = '';
  for (const ch of s) {
    const c = ch.codePointAt(0);
    if (ch === '\n' || (c >= 32 && c <= 126) || SUPPORTED.has(ch)) { out += ch; continue; }
    const base = ch.normalize('NFD').replace(/[̀-ͯ]/g, '');
    out += (base && base !== ch && /^[\x20-\x7e]+$/.test(base)) ? base : '?';
  }
  return out;
}
const ENT = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ', ndash: '-', mdash: '-', hellip: '...', laquo: '"', raquo: '"', ldquo: '"', rdquo: '"', lsquo: "'", rsquo: "'", copy: '(c)', reg: '(R)', euro: '€', pound: '£', deg: '°', middot: '*', bull: '*', times: 'x', aacute: 'á', agrave: 'à', acirc: 'â', atilde: 'ã', eacute: 'é', ecirc: 'ê', iacute: 'í', oacute: 'ó', ocirc: 'ô', otilde: 'õ', uacute: 'ú', ccedil: 'ç' };
function decode(s) {
  return s.replace(/&(#x?[0-9a-f]+|[a-z]+);/gi, (m, e) => {
    if (e[0] === '#') { const n = e[1].toLowerCase() === 'x' ? parseInt(e.slice(2), 16) : parseInt(e.slice(1), 10); try { return String.fromCodePoint(n); } catch { return ''; } }
    return ENT[e.toLowerCase()] ?? ENT[e] ?? '';
  });
}
function resolveUrl(href, base) { try { const u = new URL(href, base); return /^https?:$/.test(u.protocol) ? u.href : null; } catch { return null; } }

// ---- HTML -> watch text. Links become "text[n]"; the table is appended by the caller.
function htmlToWatch(html, base) {
  const links = [], seen = new Map();
  const tm = /<title[^>]*>([\s\S]*?)<\/title>/i.exec(html); const title = tm ? decode(tm[1]).replace(/\s+/g, ' ').trim() : base;
  let h = html.replace(/<!--[\s\S]*?-->/g, '').replace(/<(script|style|noscript|svg|iframe|template|head|nav|footer|form|button|select)\b[\s\S]*?<\/\1>/gi, ' ');
  h = h.replace(/<a\b[^>]*?href\s*=\s*("([^"]*)"|'([^']*)'|([^\s>]+))[^>]*>([\s\S]*?)<\/a>/gi, (m, _a, d, s, u, inner) => {
    const text = inner.replace(/<[^>]*>/g, '').replace(/\s+/g, ' ').trim(); if (!text) return ' ';
    const url = resolveUrl(decode(d ?? s ?? u ?? ''), base); if (!url || /^#/.test(d ?? s ?? u ?? '')) return text;
    let n = seen.get(url); if (!n) { if (links.length >= MAX_LINKS) return text; links.push(url); n = links.length; seen.set(url, n); }
    return text + '[' + n + ']';
  });
  h = h.replace(/<h[1-6][^>]*>/gi, '\n\n# ').replace(/<\/h[1-6]>/gi, '\n').replace(/<li[^>]*>/gi, '\n- ').replace(/<(br|hr)\b[^>]*>/gi, '\n').replace(/<\/(p|div|tr|ul|ol|table|section|article|blockquote|pre)>/gi, '\n').replace(/<[^>]*>/g, ' ');
  let text = decode(h).replace(/[ \t\r\f\v]+/g, ' ').replace(/ ?\n ?/g, '\n').replace(/\n{3,}/g, '\n\n').trim();
  return { title, text, links };
}
function pack(seq, url, title, text, links) {
  let body = normalize(text); if (body.length > MAX_BODY) body = body.slice(0, MAX_BODY) + '\n[...cortado]';
  const used = links.filter((u, i) => body.includes('[' + (i + 1) + ']'));       // keep numbering stable: only drop links we cut off
  let out = 'W1\nS' + seq + '\nU' + normalize(url).slice(0, 120) + '\nT' + normalize(title).slice(0, 80) + '\n' + body + '\n';
  const tail = links.map((u, i) => 'L' + (i + 1) + '\t' + u.slice(0, 200)).filter((_, i) => body.includes('[' + (i + 1) + ']')).join('\n');
  void used; return Buffer.from(out + '\u0001\n' + tail + '\n', 'utf8');
}
async function get(url) {
  const ctl = new AbortController(), t = setTimeout(() => ctl.abort(), 15000);
  try { const r = await fetch(url, { signal: ctl.signal, redirect: 'follow', headers: { 'User-Agent': 'Mozilla/5.0 (compatible; Fit3Bridge/1.0)', 'Accept': 'text/html,application/xhtml+xml,text/plain;q=0.9,*/*;q=0.5', 'Accept-Language': 'pt-BR,pt;q=0.9,en;q=0.8' } });
    const buf = Buffer.from(await r.arrayBuffer()); const ct = r.headers.get('content-type') || '';
    const m = /charset=([\w-]+)/i.exec(ct) || /<meta[^>]+charset=["']?([\w-]+)/i.exec(buf.subarray(0, 2048).toString('latin1'));
    let enc = (m ? m[1] : 'utf-8').toLowerCase(); try { new TextDecoder(enc); } catch { enc = 'utf-8'; }
    return { url: r.url, ct, html: new TextDecoder(enc).decode(buf), status: r.status };
  } finally { clearTimeout(t); }
}
async function search(q, seq) {
  const r = await get('https://html.duckduckgo.com/html/?q=' + encodeURIComponent(q)); const results = [];
  const re = /<a[^>]*class="[^"]*result__a[^"]*"[^>]*href="([^"]+)"[^>]*>([\s\S]*?)<\/a>[\s\S]*?(?:class="[^"]*result__snippet[^"]*"[^>]*>([\s\S]*?)<\/a>)?/gi; let m;
  while ((m = re.exec(r.html)) && results.length < 12) {
    let href = decode(m[1]); const u = /[?&]uddg=([^&]+)/.exec(href); if (u) href = decodeURIComponent(u[1]); else if (href.startsWith('//')) href = 'https:' + href;
    const url = resolveUrl(href, 'https://duckduckgo.com/'); if (!url) continue;
    results.push({ url, title: decode(m[2].replace(/<[^>]*>/g, '')).replace(/\s+/g, ' ').trim(), snip: decode((m[3] || '').replace(/<[^>]*>/g, '')).replace(/\s+/g, ' ').trim() });
  }
  const links = results.map(x => x.url); let text = 'Resultados para "' + q + '"\n\n';
  results.forEach((x, i) => { text += (i + 1) + '. ' + x.title + '[' + (i + 1) + ']\n' + (x.snip ? '   ' + x.snip.slice(0, 160) + '\n' : '') + '\n'; });
  if (!results.length) text += '(nenhum resultado)';
  return pack(seq, 'busca:' + q, 'Busca: ' + q, text, links);
}
async function page(target, seq) {
  const isUrl = /^https?:\/\//i.test(target) || /^[\w-]+(\.[\w-]+)+(\/.*)?$/.test(target) && !/\s/.test(target);
  if (!isUrl) return search(target, seq);
  const url = /^https?:\/\//i.test(target) ? target : 'https://' + target; const r = await get(url);
  if (/text\/plain/i.test(r.ct)) return pack(seq, r.url, r.url, r.html, []);
  const w = htmlToWatch(r.html, r.url); return pack(seq, r.url, w.title, w.text, w.links);
}
const server = http.createServer(async (req, res) => {
  const u = new URL(req.url, 'http://127.0.0.1');
  try {
    if (u.pathname === '/' || u.pathname === '/index.html') { res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }); return res.end(fs.readFileSync(path.join(__dirname, 'bridge.html'))); }
    if (u.pathname === '/api/page') {
      const seq = parseInt(u.searchParams.get('seq') || '1', 10) || 1; const q = (u.searchParams.get('q') || '').trim();
      const buf = await page(q, seq); res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'X-Bytes': String(buf.length), 'Access-Control-Allow-Origin': '*' }); return res.end(buf);
    }
    if (u.pathname === '/api/b') {
      const seq = parseInt(u.searchParams.get('seq') || '1', 10) || 1, lines = (u.searchParams.get('cmd') || '').split('\n').filter(Boolean);
      let buf;
      try { remote = remote || new Browser(); buf = await remote.frame(seq, lines); }
      catch (e) { console.log('browser error:', e.message); const idx = new Uint8Array(FW * FH); buf = buildFrame(seq, 1, 'Erro: ' + String(e.message).slice(0, 30), '', { pal: [0xFFFF].concat(new Array(15).fill(0)), idx }); remote = null; }
      res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'X-Bytes': String(buf.length), 'Access-Control-Allow-Origin': '*' }); return res.end(buf);
    }
    res.writeHead(404); res.end('not found');
  } catch (e) {
    const msg = 'Erro: ' + String(e.message || e).slice(0, 160); const buf = pack(parseInt(u.searchParams.get('seq') || '1', 10) || 1, u.searchParams.get('q') || '', 'Erro', msg, []);
    res.writeHead(200, { 'Content-Type': 'application/octet-stream' }); res.end(buf);
  }
});
if (require.main === module) server.listen(PORT, '127.0.0.1', () => console.log('Fit3 web bridge: http://127.0.0.1:' + PORT + '/'));
for (const sig of ['SIGINT', 'SIGTERM']) process.on(sig, () => { try { remote && remote.close(); } catch {} process.exit(0); });
process.on('exit', () => { try { remote && remote.proc && remote.proc.kill(); } catch {} });   // only the Chrome this server started
module.exports = { htmlToWatch, pack, normalize, page };
