// Groq chat for the Fit3 proxy. The API key stays on the PC: put it in webbridge/groq.key (one line) or in the GROQ_API_KEY environment variable.
// Optional webbridge/groq.cfg (key=value lines):  model=openai/gpt-oss-20b   max_tokens=900   system=...   (model names change: list them with GET /openai/v1/models or see console.groq.com/docs/models)
// Returns the answer in the watch's page format ("W1\nS1\nU\nT<title>\n<body>\n\x01\n") so the reader shows it like any page.
'use strict';
const fs = require('fs'), path = require('path');
const { normalize } = require('./server.js');

const BASE = (process.env.GROQ_BASE || 'https://api.groq.com/openai/v1').replace(/\/$/, '');
const SYSTEM = 'You are an assistant shown on a tiny smartwatch screen (about 31 characters per line, plain text only). '
  + 'Answer in the same language as the user. Be brief and direct: a few short sentences, no markdown, no tables, no emojis.';

function readCfg() {
  const cfg = { model: 'openai/gpt-oss-20b', max_tokens: '900', system: SYSTEM };
  try { for (const ln of fs.readFileSync(path.join(__dirname, 'groq.cfg'), 'utf8').split(/\r?\n/)) { const m = /^\s*([a-z_]+)\s*=\s*(.*?)\s*$/i.exec(ln); if (m && !ln.trim().startsWith('#')) cfg[m[1].toLowerCase()] = m[2]; } } catch {}
  return cfg;
}
function readKey() {
  if (process.env.GROQ_API_KEY) return process.env.GROQ_API_KEY.trim();
  try { return fs.readFileSync(path.join(__dirname, 'groq.key'), 'utf8').trim(); } catch { return ''; }
}
const stripMarkdown = (s) => s.replace(/```[a-z]*\n?/gi, '').replace(/\*\*([^*]+)\*\*/g, '$1').replace(/__([^_]+)__/g, '$1').replace(/^#{1,6}\s*/gm, '').replace(/`([^`]+)`/g, '$1').replace(/^\s*[*-]\s+/gm, '- ');
function page(title, body) { return Buffer.from('W1\nS1\nU\nT' + normalize(title).slice(0, 40) + '\n' + normalize(body).slice(0, 18000) + '\n\u0001\n', 'utf8'); }

// session = { hist: [] } is kept per watch connection, so follow-up questions have context (reset when the connection drops)
async function askGroq(question, session) {
  const key = readKey();
  if (!key) return page('IA', 'A chave do Groq nao esta configurada.\n\nNo PC, crie o arquivo webbridge/groq.key com a sua chave (uma linha) e reinicie o proxy.\n\nGroq API key is not set: create webbridge/groq.key and restart the proxy.');
  const cfg = readCfg(); session.hist = session.hist || [];
  const messages = [{ role: 'system', content: cfg.system }, ...session.hist.slice(-8), { role: 'user', content: question }];
  const ctl = new AbortController(), t = setTimeout(() => ctl.abort(), 40000);
  try {
    const r = await fetch(BASE + '/chat/completions', { method: 'POST', signal: ctl.signal, headers: { 'Authorization': 'Bearer ' + key, 'Content-Type': 'application/json' },
      body: JSON.stringify(Object.assign({ model: cfg.model, messages, max_tokens: parseInt(cfg.max_tokens, 10) || 900, temperature: 0.6 }, /gpt-oss/.test(cfg.model) ? { reasoning_effort: 'low' } : {})) });   // gpt-oss models reason first: keep that short so the answer fits the token budget
    const txt = await r.text(); let j = null; try { j = JSON.parse(txt); } catch {}
    if (!r.ok) return page('IA', 'Erro do Groq (' + r.status + '): ' + ((j && j.error && j.error.message) || txt.slice(0, 300)));
    const answer = stripMarkdown(String((j && j.choices && j.choices[0] && j.choices[0].message && j.choices[0].message.content) || '(sem resposta)').replace(/<think>[\s\S]*?<\/think>/g, '')).trim();
    session.hist.push({ role: 'user', content: question }, { role: 'assistant', content: answer }); session.hist = session.hist.slice(-12);
    return page('IA', 'Voce: ' + question + '\n\n' + answer);
  } catch (e) {
    return page('IA', 'Nao consegui falar com o Groq: ' + (e && e.name === 'AbortError' ? 'tempo esgotado' : (e && e.message) || e));
  } finally { clearTimeout(t); }
}
module.exports = { askGroq };
