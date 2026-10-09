"""app_sim.py - end-to-end test of the whole watch app (Internet-mode web reader) on the PC.

Runs inject/app_dut.exe (the real reader + glue + netstack with a FAKE Bluetooth layer) against phone_sim.Phone (a simulated NAP phone), the real
webbridge/proxy.js, and a tiny local web server. It opens a page through the whole chain and checks the text that ends up in the reader.

   python app_sim.py            (needs inject/app_dut.exe built with the test net_cfg.h, node on the PATH)
"""
import http.server, os, struct, subprocess, sys, threading, time
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import phone_sim as ps

APP = os.path.join(HERE, '..', 'inject', 'app_dut.exe')
PAGE = b'<html><head><title>Teste Fit3</title></head><body><h1>Ola mundo</h1><p>Este e um paragrafo de teste com um <a href="/outra">link para outra pagina</a> e acentos: ac\xc3\xa7\xc3\xa3o.</p><ul><li>item um</li><li>item dois</li></ul></body></html>'
PAGE2 = b'<html><head><title>Segunda</title></head><body><p>Voce chegou na segunda pagina.</p></body></html>'


class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = PAGE2 if self.path.startswith('/outra') else PAGE
        self.send_response(200); self.send_header('Content-Type', 'text/html; charset=utf-8'); self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass


def start_proxy(keyhex, env=None):
    """FIT3_PROXY=java runs the Android app's core (com.fit3.hub.TestServer) instead of webbridge/proxy.js."""
    if os.environ.get('FIT3_PROXY') == 'java':
        jhome = os.environ.get('JAVA_HOME', r'C:\Program Files\Eclipse Adoptium\jdk-17.0.20.101-hotspot')
        cmd = [os.path.join(jhome, 'bin', 'java'), '-Dgroq.base=http://127.0.0.1:18081', '-cp', os.path.join(HERE, '..', 'android', 'out_pc'), 'com.fit3.hub.TestServer', '8788', keyhex, 'testkey']
        return subprocess.Popen(cmd, stdout=subprocess.DEVNULL)
    return subprocess.Popen(['node', os.path.join(HERE, '..', 'webbridge', 'proxy.js'), '8788', keyhex], stdout=subprocess.DEVNULL, env=env)


class AppDut(ps.Dut):
    def __init__(self, *args):
        self.p = subprocess.Popen([APP, *args], stdin=subprocess.PIPE, stdout=subprocess.PIPE); self.now = 1000


def status(dut):
    for t, d in dut.call('Q'):
        if t == 's': return dict(phase=d[0], net=d[1], sc=d[2], mode=d[3], links=d[4], ev=d[5], lines=(d[6] << 8) | d[7], title=d[16:64].split(b'\0')[0].decode('utf8', 'replace'), text=d[64:].split(b'\0')[0].decode('utf8', 'replace'))


def run(name, loss=0.0, shot=None):
    fails = []; dut = AppDut(); ph = ps.Phone(dut, 8788, loss=loss, seed=3)

    def step(ms=50):
        dut.now += ms; ph.handle_out(dut.call('K', struct.pack('>I', dut.now))); ph.pump_sessions(dut.now); time.sleep(0.0005)

    def until(cond, limit_ms, what):
        t = 0
        while t < limit_ms:
            if cond(): return True
            step(); t += 50
        fails.append('timeout: ' + what); return False

    step(); step()
    if until(lambda: status(dut)['phase'] == 4, 60000, 'app reaches READY (BT open -> BNEP -> DHCP -> proxy)'):
        until(lambda: status(dut)['mode'] == 0, 5000, 'welcome page shown')
        s = status(dut)
        if shot: dut.call('P', (shot + '_ready.ppm').encode())
        ph.handle_out(dut.call('G', b'http://127.0.0.1:18080/'))
        until(lambda: status(dut)['mode'] == 0 and status(dut)['title'] != '' and status(dut)['title'] != 'Internet', 60000, 'page loaded')
        s = status(dut)
        if s['title'] != 'Teste Fit3': fails.append('wrong title %r' % s['title'])
        if s['links'] != 1: fails.append('expected 1 link, got %d' % s['links'])
        if shot: dut.call('P', (shot + '_page.ppm').encode())
        ph.handle_out(dut.call('G', b'http://127.0.0.1:18080/outra'))
        until(lambda: status(dut)['title'] == 'Segunda', 60000, 'second page loaded')
        if status(dut)['title'] != 'Segunda': fails.append('second page title %r' % status(dut)['title'])
    print('%-30s loss=%.2f frames=%4d dropped=%3d virtual=%6d ms -> %s' % (name, loss, ph.sent, ph.dropped, dut.now, 'OK' if not fails else 'FAIL ' + '; '.join(fails)))
    for t in ph.sess.values():
        try: t.sock.close()
        except Exception: pass
    dut.close(); return not fails


if __name__ == '__main__' and 'ai' not in sys.argv[1:] and 'dial' not in sys.argv[1:]:
    keyhex = os.path.join(HERE, 'test.key.hex')
    if not os.path.exists(keyhex): open(keyhex, 'w').write(open(os.path.join(HERE, 'test.key'), 'rb').read().hex() + '\n')
    web = http.server.ThreadingHTTPServer(('127.0.0.1', 18080), H); threading.Thread(target=web.serve_forever, daemon=True).start()
    node = start_proxy(keyhex); time.sleep(1.5)
    try:
        r = [run('app: clean link', shot=os.path.join(HERE, 'app'))]
        r.append(run('app: lossy link', loss=0.06))
    finally:
        node.kill(); web.shutdown()
    print('ALL OK' if all(r) else 'SOME FAILED'); sys.exit(0 if all(r) else 1)


# ---------------------------------------------------------------- Groq AI scenario (fake Groq server, real proxy)
import json
class FakeGroq(http.server.BaseHTTPRequestHandler):
    seen = []
    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        FakeGroq.seen.append((self.headers.get('Authorization'), body))
        ans = '**Paris** e a capital da Franca.\n\n- item um\n- item dois' if len(body['messages']) < 3 else 'Cerca de **2 milhoes** de pessoas.'
        out = json.dumps({'choices': [{'message': {'content': ans}}]}).encode()
        self.send_response(200); self.send_header('Content-Type', 'application/json'); self.send_header('Content-Length', str(len(out))); self.end_headers(); self.wfile.write(out)
    def log_message(self, *a): pass


def run_ai():
    fails = []; dut = AppDut('ai'); ph = ps.Phone(dut, 8788, loss=0.0, seed=5)
    def step(ms=50):
        dut.now += ms; ph.handle_out(dut.call('K', struct.pack('>I', dut.now))); ph.pump_sessions(dut.now); time.sleep(0.0005)
    def until(cond, limit_ms, what):
        t = 0
        while t < limit_ms:
            if cond(): return True
            step(); t += 50
        fails.append('timeout: ' + what); return False
    step(); step()
    until(lambda: status(dut)['phase'] == 4, 60000, 'AI app READY'); until(lambda: status(dut)['mode'] == 0, 5000, 'welcome page')
    if status(dut)['title'] != 'IA': fails.append('AI welcome title %r' % status(dut)['title'])
    ph.handle_out(dut.call('G', b'Qual a capital da Franca?'))
    until(lambda: status(dut)['mode'] == 0 and 'Franca' in status(dut)['text'], 60000, 'AI answer 1')
    s = status(dut)
    if '**' in s['text'] or 'Paris' not in (s['text'] + 'Paris'): fails.append('markdown not stripped: %r' % s['text'])
    ph.handle_out(dut.call('G', b'E quantos habitantes?'))
    until(lambda: len(FakeGroq.seen) >= 2, 60000, 'second question reached Groq'); until(lambda: status(dut)['mode'] == 0 and status(dut)['text'].startswith('Voce: E quantos'), 60000, 'AI answer 2')
    if len(FakeGroq.seen) < 2: fails.append('Groq saw %d requests' % len(FakeGroq.seen))
    else:
        (auth1, b1), (auth2, b2) = FakeGroq.seen[0], FakeGroq.seen[1]
        if auth1 != 'Bearer testkey': fails.append('bad auth header %r' % auth1)
        if [m['role'] for m in b1['messages']] != ['system', 'user']: fails.append('first request roles %r' % [m['role'] for m in b1['messages']])
        if [m['role'] for m in b2['messages']] != ['system', 'user', 'assistant', 'user']: fails.append('follow-up lost the context: %r' % [m['role'] for m in b2['messages']])
        if '**' in b2['messages'][2]['content']: fails.append('markdown kept in the stored history')
    print('%-30s frames=%4d virtual=%6d ms -> %s' % ('AI: Groq via proxy', ph.sent, dut.now, 'OK' if not fails else 'FAIL ' + '; '.join(fails)))
    for t in ph.sess.values():
        try: t.sock.close()
        except Exception: pass
    dut.close(); return not fails


def main_ai():
    keyhex = os.path.join(HERE, 'test.key.hex')
    groq = http.server.ThreadingHTTPServer(('127.0.0.1', 18081), FakeGroq); threading.Thread(target=groq.serve_forever, daemon=True).start()
    env = dict(os.environ, GROQ_API_KEY='testkey', GROQ_BASE='http://127.0.0.1:18081')
    node = start_proxy(keyhex, env); time.sleep(1.5)
    try: ok = run_ai()
    finally: node.kill(); groq.shutdown()
    print('AI ALL OK' if ok else 'AI FAILED'); return ok


if __name__ == '__main__' and 'ai' in sys.argv[1:]:
    sys.exit(0 if main_ai() else 1)

def run_dial(number='5511987654321', fail_number=None):
    fails = []; dut = AppDut('d'); ph = ps.Phone(dut, 8788, loss=0.0, seed=7)
    def step(ms=50):
        dut.now += ms; ph.handle_out(dut.call('K', struct.pack('>I', dut.now))); ph.pump_sessions(dut.now); time.sleep(0.0005)
    def until(cond, limit_ms, what):
        t = 0
        while t < limit_ms:
            if cond(): return True
            step(); t += 50
        fails.append('timeout: ' + what); return False
    def tap(x, y): ph.handle_out(dut.call('T', struct.pack('>HH', x, y)))
    def key(ch):
        i = '123456789*0#'.index(ch); r, c = divmod(i, 3); tap(8 + c * 82 + 39, 96 + r * 62 + 29)
    step(); step()
    until(lambda: status(dut)['phase'] == 4, 60000, 'dialer READY'); until(lambda: status(dut)['mode'] == 3, 5000, 'keypad shown')
    if status(dut)['mode'] != 3: fails.append('not on the keypad (mode %d)' % status(dut)['mode'])
    for ch in number: key(ch); step(20)
    tap(47, 370); step(20)                                        # backspace
    key(number[-1]); step(20)                                     # ... and type it again
    tap(170, 370)                                                 # CALL
    until(lambda: status(dut)['mode'] == 0 and status(dut)['title'] == 'Discador', 60000, 'call answered')
    s = status(dut); print('   result page: title=%r text=%r' % (s['title'], s['text'][:40].encode('ascii','replace').decode()))
    if not (s['text'].startswith('Chamando ' + number) or s['text'].startswith('Numero ' + number)): fails.append('unexpected result %r' % s['text'])
    tap(100, 200); step(50)
    if status(dut)['mode'] != 3: fails.append('tap did not return to the keypad (mode %d)' % status(dut)['mode'])
    print('%-30s frames=%4d virtual=%6d ms -> %s' % ('Dialer via proxy', ph.sent, dut.now, 'OK' if not fails else 'FAIL ' + '; '.join(fails)))
    for t in ph.sess.values():
        try: t.sock.close()
        except Exception: pass
    dut.close(); return not fails


def main_dial():
    keyhex = os.path.join(HERE, 'test.key.hex')
    node = start_proxy(keyhex); time.sleep(1.5)
    try: ok = run_dial()
    finally: node.kill()
    print('DIAL ALL OK' if ok else 'DIAL FAILED'); return ok


if __name__ == '__main__' and 'dial' in sys.argv[1:]:
    sys.exit(0 if main_dial() else 1)
