// End-to-end test of the PC mouse/keyboard panel: a helper Chrome (port 9444) drives bridge.html, which drives the server's remote Chrome.
'use strict';
const { spawn } = require('child_process'), http = require('http');
const { Browser } = require('./browser.js');
const delay = ms => new Promise(r => setTimeout(r, ms));
const site = http.createServer((q, r) => { r.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
  r.end('<meta name=viewport content="width=device-width,initial-scale=1"><title>site</title><body style="margin:0"><input id=i autofocus style="position:fixed;inset:0;width:100%;height:100%;font:24px sans-serif;box-sizing:border-box">' +
    '<div style="height:3000px"></div><script>document.addEventListener("click",e=>{document.title="click "+Math.round(e.clientX)+","+Math.round(e.clientY)});document.getElementById("i").addEventListener("input",e=>{document.title="input "+e.target.value});document.addEventListener("keydown",e=>{if(e.key==="Backspace")document.title="bksp "+document.getElementById("i").value})</script></body>'); }).listen(8095);
(async () => {
  const srv = spawn(process.execPath, ['server.js'], { cwd: __dirname, stdio: 'ignore' }); await delay(1500);
  const h = new Browser({ port: 9444 }); await h.start(); await h.send('Emulation.clearDeviceMetricsOverride'); await h.send('Emulation.setDeviceMetricsOverride', { width: 900, height: 1100, deviceScaleFactor: 1, mobile: false });
  const nav = async u => { const l = h.once('Page.loadEventFired', 8000); await h.send('Page.navigate', { url: u }); await l; await delay(400); };
  const ev = async x => (await h.send('Runtime.evaluate', { expression: x, returnByValue: true, awaitPromise: true })).result.value;
  await nav('http://127.0.0.1:8787/');
  console.log('page has PC panel:', await ev("!!document.getElementById('view') && typeof pcRun"));
  await ev("pcRun(['go http://127.0.0.1:8095/'])"); await delay(5000);
  const nonBlank = await ev("(()=>{const d=document.getElementById('view').getContext('2d').getImageData(0,0,256,340).data;let n=0;for(let i=0;i<d.length;i+=4)if(d[i]!==34)n++;return n})()");
  console.log('preview drawn (non-placeholder pixels):', nonBlank, ' address bar:', await ev("document.getElementById('addr').value"));
  const rc = JSON.parse(await ev("(()=>{const r=document.getElementById('view').getBoundingClientRect();return JSON.stringify([r.x,r.y,r.width,r.height])})()")); console.log('canvas rect', rc.map(Math.round));
  const cx = rc[0] + rc[2] * 0.4, cy = rc[1] + rc[3] * 0.3;
  await h.mouse('mouseMoved', cx, cy, { button: 'none', clickCount: 0 }); await h.mouse('mousePressed', cx, cy); await h.mouse('mouseReleased', cx, cy); await delay(3500);
  console.log('after mouse click -> title:', await ev('document.title'));
  for (const ch of 'Ola') { await h.send('Input.dispatchKeyEvent', { type: 'keyDown', key: ch, text: ch, windowsVirtualKeyCode: ch.charCodeAt(0) }); await h.send('Input.dispatchKeyEvent', { type: 'keyUp', key: ch, windowsVirtualKeyCode: ch.charCodeAt(0) }); }
  await delay(4500); console.log('after typing "Ola" -> title:', await ev('document.title'));
  await h.send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Backspace', code: 'Backspace', windowsVirtualKeyCode: 8 }); await h.send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Backspace', code: 'Backspace', windowsVirtualKeyCode: 8 });
  await delay(3500); console.log('after Backspace -> title:', await ev('document.title'));
  await h.mouse('mouseWheel', cx, cy, { button: 'none', clickCount: 0, deltaX: 0, deltaY: 300 }); await delay(3500);
  console.log('after wheel: no errors; address:', await ev("document.getElementById('addr').value"));
  await h.close(); srv.kill(); site.close(); process.exit(0);
})().catch(e => { console.log('ERR', e); process.exit(1); });
