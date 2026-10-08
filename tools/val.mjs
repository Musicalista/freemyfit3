import fs from 'fs';
const src=fs.readFileSync((process.env.FLASHER_DIR || '../fit3-flasher') + '/validation-worker.js','utf8');
let out; globalThis.self={}; (0,eval)(src.replace("self.onmessage","globalThis.__h"));
const buf=fs.readFileSync(process.argv[2]);
self.postMessage=m=>{ if(!m.progress) console.log(JSON.stringify(m)); };
await globalThis.__h({data:{buffer:buf.buffer.slice(buf.byteOffset,buf.byteOffset+buf.length),stock:false}});
