import os, struct, sys, time, threading
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import phone_sim as ps, app_sim as A
dut = A.AppDut(); ph = ps.Phone(dut, 8788, seed=3); ph.dns_names['site.test'] = 0x0A4D0002; ph.routes[0x0A4D0002] = ('127.0.0.1', 18080)
srv = A.http.server.ThreadingHTTPServer(('127.0.0.1', 18080), A.H); threading.Thread(target=srv.serve_forever, daemon=True).start()
def step(ms=50):
    dut.now += ms; ph.handle_out(dut.call('K', struct.pack('>I', dut.now))); ph.pump_sessions(dut.now); time.sleep(0.0005)
for _ in range(200):
    step()
    if A.status(dut)['phase'] == 4: break
print('phase', A.status(dut)['phase'])
ph.handle_out(dut.call('G', b'http://site.test/'))
last = None
for i in range(600):
    step(); s = A.status(dut); cur = (s['ds'], s['tls'], s['mode'], s['title'])
    if cur != last: print(dut.now, cur); last = cur
    if s['title'] == 'Teste Fit3': break
print('sessions', [(hex(k[2]), k[3], t.state) for k, t in ph.sess.items()])
