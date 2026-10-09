"""phone_sim.py - a simulated phone (Bluetooth NAP bridge) for testing net/netstack.c end to end on the PC.

It plays the other end of the L2CAP/BNEP link: BNEP setup + filter requests, an Ethernet segment with ARP, a DHCP server, a DNS server, ICMP, and a small TCP *server*
that terminates the watch's connections and bridges the byte stream to the real proxy (webbridge/proxy.js) over a localhost socket.
The stack under test (net_dut.exe) runs on a virtual clock, so a whole session takes a second; frames can be dropped at random to exercise retransmits.

  python phone_sim.py                  run all scenarios (needs net_dut.exe built and node on the PATH)
"""
import os, random, socket, struct, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
PROXY_JS = os.path.join(HERE, '..', 'webbridge', 'proxy.js')
GW_IP, WATCH_IP, DNS_IP, FAKE_PROXY_IP = 0xC0A82C01, 0xC0A82C17, 0xC0A82C01, 0x0A4D0001   # 192.168.44.1 / .23 / 10.77.0.1
GW_MAC = bytes.fromhex('021122334455'); BCAST = b'\xff' * 6


def csum(b):
    if len(b) % 2: b += b'\0'
    s = sum(struct.unpack('>%dH' % (len(b) // 2), b))
    while s >> 16: s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def ip_pkt(proto, src, dst, payload, ident=1):
    h = struct.pack('>BBHHHBBHII', 0x45, 0, 20 + len(payload), ident, 0x4000, 64, proto, 0, src, dst)
    return h[:10] + struct.pack('>H', csum(h)) + h[12:] + payload


def tcp_seg(src, dst, sport, dport, seq, ack, flags, win, data=b'', mss=None):
    opt = struct.pack('>BBH', 2, 4, mss) if mss else b''
    hl = 20 + len(opt)
    t = struct.pack('>HHIIBBHHH', sport, dport, seq & 0xffffffff, ack & 0xffffffff, (hl // 4) << 4, flags, win, 0, 0) + opt + data
    ph = struct.pack('>IIBBH', src, dst, 0, 6, len(t))
    return t[:16] + struct.pack('>H', csum(ph + t)) + t[18:]


class Dut:
    def __init__(self):
        exe = os.path.join(HERE, 'net_dut.exe')
        self.p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.now = 0

    def call(self, t, payload=b''):
        self.p.stdin.write(bytes([ord(t)]) + struct.pack('>H', len(payload)) + payload); self.p.stdin.flush()
        out = []
        while True:
            h = self.p.stdout.read(3)
            if len(h) < 3: raise RuntimeError('DUT died')
            n = struct.unpack('>H', h[1:])[0]; d = self.p.stdout.read(n) if n else b''
            if h[0:1] == b'z': return out
            out.append((chr(h[0]), d))

    def close(self):
        try: self.p.stdin.close(); self.p.wait(2)
        except Exception: self.p.kill()


class TcpSess:
    def __init__(self, sim, key, seq, mss):
        self.sim, self.key = sim, key
        self.sip, self.sport, self.dip, self.dport = key
        self.rcv_nxt = (seq + 1) & 0xffffffff
        self.iss = random.getrandbits(32); self.snd_una = self.iss; self.snd_nxt = (self.iss + 1) & 0xffffffff
        self.mss = mss; self.state = 'syn'; self.out = b''; self.seglen = 0; self.t0 = 0; self.sock = None; self.fin_sent = False
        self.peer_mss = 536

    def seg(self, flags, data=b'', seq=None, mss=None):
        return self.sim.send_ip(6, self.dip, self.sip, tcp_seg(self.dip, self.sip, self.dport, self.sport, self.snd_nxt if seq is None else seq, self.rcv_nxt, flags, 8192, data, mss))


class Phone:
    def __init__(self, dut, proxy_port, loss=0.0, mss=1460, seed=1):
        self.dut, self.proxy_port, self.loss, self.mss = dut, proxy_port, loss, mss
        self.rng = random.Random(seed); self.sess = {}; self.watch_mac = None; self.events = []; self.msgs = []; self.sc_err = False
        self.routes = {}                                           # fake destination IP -> (host, port) of a local test server (direct web access)
        self.dns_names = {'proxy.test': FAKE_PROXY_IP}; self.dropped = 0; self.sent = 0; self.pings = 0; self.filter_acked = False; self.arp_replied = False

    # ---- link ----
    def to_watch(self, sdu):
        self.sent += 1
        if self.rng.random() < self.loss: self.dropped += 1; return
        self.handle_out(self.dut.call('F', sdu))

    def eth(self, dst, proto, payload): return b'\x00' + dst + GW_MAC + struct.pack('>H', proto) + payload
    def send_ip(self, proto, src, dst, payload):
        if self.watch_mac is None: return
        self.to_watch(self.eth(self.watch_mac, 0x0800, ip_pkt(proto, src, dst, payload)))

    def handle_out(self, outs):
        for t, d in outs:
            if t == 'f':
                if self.rng.random() < self.loss: self.dropped += 1; continue
                self.from_watch(d)
            elif t == 'm': self.msgs.append(d)
            elif t == 'e': self.sc_err = True

    # ---- frames from the watch ----
    def from_watch(self, f):
        if f[0] == 0x01:                                           # BNEP control
            if f[1] == 0x01: self.to_watch(bytes([0x01, 0x02, 0, 0])); self.to_watch(bytes([0x01, 0x05, 0, 0]))   # setup ok + a multicast filter request
            elif f[1] == 0x06 and f[2:4] == b'\0\0': self.filter_acked = True
            return
        if f[0] != 0x00 or len(f) < 15: return
        dst, src, proto = f[1:7], f[7:13], struct.unpack('>H', f[13:15])[0]; body = f[15:]
        self.watch_mac = src
        if proto == 0x0806 and len(body) >= 28:
            op = struct.unpack('>H', body[6:8])[0]; tpa = struct.unpack('>I', body[24:28])[0]
            if op == 1 and tpa == GW_IP:
                self.arp_replied = True
                self.to_watch(self.eth(src, 0x0806, struct.pack('>HHBBH', 1, 0x0800, 6, 4, 2) + GW_MAC + struct.pack('>I', GW_IP) + src + body[14:18]))
        elif proto == 0x0800: self.ip_in(body)

    def ip_in(self, p):
        ihl = (p[0] & 15) * 4; tot = struct.unpack('>H', p[2:4])[0]; proto = p[9]; src, dst = struct.unpack('>II', p[12:20]); pl = p[ihl:tot]
        if proto == 17:
            sp, dp = struct.unpack('>HH', pl[:4]); data = pl[8:]
            if dp == 67: self.dhcp(data)
            elif dp == 53: self.dns(src, sp, data)
        elif proto == 1 and pl[0] == 0: self.pings += 1
        elif proto == 6: self.tcp_in(src, dst, pl)

    # ---- DHCP ----
    def dhcp(self, d):
        if len(d) < 241: return
        xid = d[4:8]; chaddr = d[28:34]; opts = {}; o = 240
        while o < len(d) and d[o] != 255:
            if d[o] == 0: o += 1; continue
            opts[d[o]] = d[o + 2:o + 2 + d[o + 1]]; o += 2 + d[o + 1]
        mt = opts.get(53, b'\0')[0]
        if mt not in (1, 3): return
        rt = 2 if mt == 1 else 5
        r = bytearray(240); r[0] = 2; r[1] = 1; r[2] = 6; r[4:8] = xid; r[10:12] = b'\x80\x00'; r[16:20] = struct.pack('>I', WATCH_IP); r[20:24] = struct.pack('>I', GW_IP); r[28:34] = chaddr; r[236:240] = b'\x63\x82\x53\x63'
        r += bytes([53, 1, rt, 54, 4]) + struct.pack('>I', GW_IP) + bytes([51, 4]) + struct.pack('>I', 3600) + bytes([1, 4, 255, 255, 255, 0, 3, 4]) + struct.pack('>I', GW_IP) + bytes([6, 4]) + struct.pack('>I', DNS_IP) + bytes([255])
        udp = struct.pack('>HHHH', 67, 68, 8 + len(r), 0) + bytes(r)
        self.to_watch(self.eth(BCAST, 0x0800, ip_pkt(17, GW_IP, 0xffffffff, udp)))

    # ---- DNS ----
    def dns(self, src, sport, q):
        if len(q) < 12: return
        o = 12; name = []
        while q[o]: name.append(q[o + 1:o + 1 + q[o]].decode()); o += 1 + q[o]
        o += 5; qsec = q[12:o]; nm = '.'.join(name); ip = self.dns_names.get(nm)
        hdr = q[:2] + (b'\x81\x80' if ip else b'\x81\x83') + b'\x00\x01' + (b'\x00\x01' if ip else b'\x00\x00') + b'\0\0\0\0'
        ans = (b'\xc0\x0c' + struct.pack('>HHIH', 1, 1, 60, 4) + struct.pack('>I', ip)) if ip else b''
        resp = hdr + qsec + ans
        self.send_ip(17, GW_IP, src, struct.pack('>HHHH', 53, sport, 8 + len(resp), 0) + resp)

    def ping(self):
        icmp = b'\x08\x00\x00\x00\x12\x34\x00\x01' + b'abcdefgh'
        icmp = icmp[:2] + struct.pack('>H', csum(icmp)) + icmp[4:]
        self.send_ip(1, GW_IP, WATCH_IP, icmp)

    # ---- TCP server bridged to the real proxy ----
    def tcp_in(self, sip, dip, s):
        sport, dport, seq, ack = struct.unpack('>HHII', s[:12]); hl = (s[12] >> 4) * 4; fl = s[13]; data = s[hl:]
        key = (sip, sport, dip, dport); t = self.sess.get(key)
        if fl & 2 and t is None:
            if dip not in (FAKE_PROXY_IP, GW_IP) and dip not in self.routes and dip != 0x7f000001: return     # the phone itself (gateway) is also a valid proxy address
            t = TcpSess(self, key, seq, self.mss); self.sess[key] = t
            for o in range(20, hl - 1):
                if s[o] == 2 and s[o + 1] == 4: t.peer_mss = struct.unpack('>H', s[o + 2:o + 4])[0]; break
            target = self.routes.get(dip) or (('127.0.0.1', dport) if dip == 0x7f000001 else ('127.0.0.1', self.proxy_port))
            t.sock = socket.create_connection(target); t.sock.setblocking(False)
            t.seg(0x12, b'', t.iss, self.mss); return
        if fl & 2 and t is not None and t.state == 'syn': t.seg(0x12, b'', t.iss, self.mss); return            # retransmitted SYN
        if t is None: return
        if t.state == 'syn' and fl & 0x10 and ack == ((t.iss + 1) & 0xffffffff): t.state = 'est'; t.snd_una = t.snd_nxt = (t.iss + 1) & 0xffffffff   # the SYN consumed one sequence number
        if t.state in ('est', 'fin') and fl & 0x10 and t.seglen and ack == ((t.snd_una + t.seglen) & 0xffffffff):
            t.snd_una = ack; t.out = t.out[t.seglen:]; t.seglen = 0
        if data or fl & 1:
            if seq == t.rcv_nxt:
                if data: t.rcv_nxt = (t.rcv_nxt + len(data)) & 0xffffffff; t.sock.sendall(data)
                if fl & 1: t.rcv_nxt = (t.rcv_nxt + 1) & 0xffffffff; t.state = 'closed'
            t.seg(0x10)

    def pump_sessions(self, now):
        for t in list(self.sess.values()):
            if t.sock and t.state == 'est':
                try:
                    d = t.sock.recv(65536)
                    if d: t.out += d
                except BlockingIOError: pass
                except OSError: pass
            if t.state == 'est' and t.out:
                if not t.seglen:
                    n = min(len(t.out), t.peer_mss, self.mss); t.seglen = n; t.snd_nxt = (t.snd_una + n) & 0xffffffff; t.t0 = now; t.seg(0x18, t.out[:n], t.snd_una)
                elif now - t.t0 > 800:
                    t.t0 = now; t.seg(0x18, t.out[:t.seglen], t.snd_una)


def run(name, loss=0.0, mss=1460, psk=None, expect_sc_ok=True, proxy_port=18788):
    dut = Dut(); ph = Phone(dut, proxy_port, loss=loss, mss=mss)
    key = open(os.path.join(HERE, 'test.key'), 'rb').read() if psk is None else psk
    fails = []
    def step(ms=20):
        dut.now += ms; ph.handle_out(dut.call('K', struct.pack('>I', dut.now))); ph.pump_sessions(dut.now); time.sleep(0.0005)
    def until(cond, limit_ms, what):
        t = 0
        while t < limit_ms:
            if cond(): return True
            step(); t += 20
        fails.append('timeout: ' + what); return False
    def status():
        for t, d in dut.call('Q'):
            if t == 's': return d
    ph.handle_out(dut.call('U'))
    ok = until(lambda: status()[0] == 4, 30000, 'DHCP/ARP -> READY')
    if ok:
        s = status(); ip = struct.unpack('>I', s[4:8])[0]
        if ip != WATCH_IP: fails.append('wrong ip %x' % ip)
        if not ph.filter_acked: fails.append('multicast filter request not answered')
        for _ in range(8):                                          # ICMP is not retransmitted, so a lossy link may need several pings
            ph.ping()
            if until(lambda: ph.pings > 0, 400, 'ICMP echo reply') or ph.pings: break
        if ph.pings: fails = [f for f in fails if not f.startswith('timeout: ICMP')]
        else: fails.append('no ICMP echo reply after 8 pings')
        r = dut.call('S', struct.pack('>H', 18788 if False else 8788) + key + b'proxy.test'); ph.handle_out(r)
        got_ready = until(lambda: status()[2] == 4 or ph.sc_err, 30000, 'secure channel handshake')
        if expect_sc_ok:
            if status()[2] != 4: fails.append('secure channel not ready (state %d)' % status()[2])
            else:
                def ask(req, want_total):
                    ph.msgs.clear()
                    t = 0
                    while True:
                        r = dut.call('W', req); ph.handle_out(r)
                        if any(k == 'r' and v == b'\x00' for k, v in r): break
                        step(); t += 20
                        if t > 5000: fails.append('send busy'); return b''
                    until(lambda: bool(ph.msgs) and ph.msgs[-1][0] == 1, 40000, 'reply to ' + req[:12].decode())
                    return b''.join(m[1:] for m in ph.msgs)
                a = ask(b'Ehello world', 11)
                if a != b'hello world': fails.append('echo mismatch %r' % a[:30])
                b = ask(b'B3000', 3000)
                if b != bytes(i % 251 for i in range(3000)): fails.append('bulk mismatch len=%d' % len(b))
        else:
            if status()[2] != 5: fails.append('wrong key must end in SC_ERR, got state %d' % status()[2])
    print('%-34s loss=%.2f mss=%4d  frames=%4d dropped=%3d  virtual=%5d ms  -> %s' % (name, loss, mss, ph.sent, ph.dropped, dut.now, 'OK' if not fails else 'FAIL ' + '; '.join(fails)))
    for t in ph.sess.values():
        try: t.sock.close()
        except Exception: pass
    dut.close()
    return not fails


if __name__ == '__main__':
    if not os.path.exists(os.path.join(HERE, 'test.key')): open(os.path.join(HERE, 'test.key'), 'wb').write(os.urandom(32))
    keyhex = open(os.path.join(HERE, 'test.key'), 'rb').read().hex()
    open(os.path.join(HERE, 'test.key.hex'), 'w').write(keyhex + '\n')
    node = subprocess.Popen(['node', PROXY_JS, '8788', os.path.join(HERE, 'test.key.hex')], stdout=subprocess.DEVNULL)
    time.sleep(1.0)
    try:
        # the fake proxy IP's TCP sessions are bridged to the real proxy on 127.0.0.1:8788
        results = [run('clean link', proxy_port=8788), run('small MSS (536)', mss=536, proxy_port=8788), run('lossy link', loss=0.08, proxy_port=8788),
                   run('very lossy link', loss=0.15, proxy_port=8788), run('wrong key', psk=os.urandom(32), expect_sc_ok=False, proxy_port=8788)]
    finally:
        node.kill()
    print('ALL OK' if all(results) else 'SOME FAILED'); sys.exit(0 if all(results) else 1)
