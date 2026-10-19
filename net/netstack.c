#include "netstack.h"
#include "chacha.h"

/* ---------- tiny libc-free helpers ---------- */
static void ncpy(void *d, const void *s, int n) { uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s; while (n-- > 0) *a++ = *b++; }
static void nset(void *d, int v, int n) { uint8_t *a = (uint8_t *)d; while (n-- > 0) *a++ = (uint8_t)v; }
static int ncmp(const void *x, const void *y, int n) { const uint8_t *a = (const uint8_t *)x, *b = (const uint8_t *)y; while (n-- > 0) { if (*a != *b) return *a - *b; a++; b++; } return 0; }
static uint16_t nrd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t nrd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static void nwr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void nwr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static int nlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static uint32_t ncsum_add(uint32_t sum, const uint8_t *d, int len) {
    while (len > 1) { sum += (uint32_t)((d[0] << 8) | d[1]); d += 2; len -= 2; }
    if (len) sum += (uint32_t)(d[0] << 8);
    return sum;
}
static uint16_t ncsum_fin(uint32_t sum) { while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16); return (uint16_t)~sum; }
static uint32_t NNOW(Net *n) { return n->ops.now(n->ops.ctx); }
static const uint8_t NBCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

uint32_t net_parse_ip(const char *s) {
    uint32_t ip = 0; int parts = 0, v = 0, digits = 0;
    for (;; s++) {
        if (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); if (++digits > 3 || v > 255) return 0; }
        else if (*s == '.' || *s == 0) { if (!digits) return 0; ip = (ip << 8) | (uint32_t)v; parts++; v = 0; digits = 0; if (*s == 0) break; }
        else return 0;
    }
    return parts == 4 ? ip : 0;
}

/* ---------- link layer (BNEP) ---------- */
#define PAY 35                                   /* offset of the IP payload in n->fb: 15 (BNEP general ethernet) + 20 (IPv4) */
static void eth_out(Net *n, const uint8_t *dst, uint16_t type, int plen) {   /* payload already at fb + 15 */
    n->fb[0] = 0x00; ncpy(n->fb + 1, dst, 6); ncpy(n->fb + 7, n->mac, 6); nwr16(n->fb + 13, type);
    n->ops.tx(n->ops.ctx, n->fb, 15 + plen);
}
static void bnep_ctl_reply(Net *n, uint8_t type, uint16_t result) {
    uint8_t m[4]; m[0] = 0x01; m[1] = type; nwr16(m + 2, result); n->ops.tx(n->ops.ctx, m, 4);
}
static void bnep_setup_req(Net *n) {
    uint8_t m[7] = {0x01, 0x01, 0x02, 0x11, 0x16, 0x11, 0x15};   /* control / SETUP_CONNECTION_REQUEST / uuid size 2 / dst NAP 0x1116 / src PANU 0x1115 */
    n->ops.tx(n->ops.ctx, m, 7);
}

/* ---------- ARP ---------- */
static const uint8_t *arp_find(Net *n, uint32_t ip) { for (int i = 0; i < 4; i++) if (n->arp[i].ok && n->arp[i].ip == ip) return n->arp[i].mac; return 0; }
static void arp_learn(Net *n, uint32_t ip, const uint8_t *mac) {
    int i; for (i = 0; i < 4; i++) if (n->arp[i].ok && n->arp[i].ip == ip) break;
    if (i == 4) { i = n->arp_next; n->arp_next = (uint8_t)((n->arp_next + 1) & 3); }
    n->arp[i].ok = 1; n->arp[i].ip = ip; ncpy(n->arp[i].mac, mac, 6);
}
static void arp_out(Net *n, uint16_t op, const uint8_t *tha, uint32_t tpa, const uint8_t *eth_dst) {
    uint8_t *a = n->fb + 15; nwr16(a, 1); nwr16(a + 2, 0x0800); a[4] = 6; a[5] = 4; nwr16(a + 6, op);
    ncpy(a + 8, n->mac, 6); nwr32(a + 14, n->ip); ncpy(a + 18, tha, 6); nwr32(a + 24, tpa);
    eth_out(n, eth_dst, 0x0806, 28);
}
static void arp_request(Net *n, uint32_t ip) { static const uint8_t z[6] = {0}; arp_out(n, 1, z, ip, NBCAST); }
static void arp_rx(Net *n, const uint8_t *a, int len) {
    if (len < 28 || nrd16(a + 2) != 0x0800 || a[4] != 6 || a[5] != 4) return;
    uint16_t op = nrd16(a + 6); uint32_t spa = nrd32(a + 14), tpa = nrd32(a + 24);
    if (spa) arp_learn(n, spa, a + 8);
    if (op == 1 && n->ip && tpa == n->ip) arp_out(n, 2, a + 8, spa, a + 8);
}

/* ---------- IPv4 ---------- */
static int ip_out(Net *n, uint8_t proto, uint32_t src, uint32_t dst, int plen, const uint8_t *dmac) {   /* payload at fb + PAY */
    uint8_t *h = n->fb + 15; const uint8_t *mac = dmac;
    if (!mac) {
        uint32_t nh = (n->mask && (dst & n->mask) != (n->ip & n->mask) && n->gw) ? n->gw : dst;
        if (dst == 0xffffffffu) mac = NBCAST; else mac = arp_find(n, nh);
        if (!mac) { arp_request(n, nh); return -1; }
    }
    h[0] = 0x45; h[1] = 0; nwr16(h + 2, 20 + plen); nwr16(h + 4, (uint32_t)(++n->ipid)); nwr16(h + 6, 0x4000); h[8] = 64; h[9] = proto; nwr16(h + 10, 0);
    nwr32(h + 12, src); nwr32(h + 16, dst); nwr16(h + 10, ncsum_fin(ncsum_add(0, h, 20)));
    { uint8_t m[6]; ncpy(m, mac, 6); eth_out(n, m, 0x0800, 20 + plen); }
    return 0;
}
static int udp_out(Net *n, uint32_t src, uint32_t dst, uint16_t sp, uint16_t dp, int plen, const uint8_t *dmac) {   /* udp payload at fb + PAY + 8 */
    uint8_t *u = n->fb + PAY; nwr16(u, sp); nwr16(u + 2, dp); nwr16(u + 4, 8 + plen); nwr16(u + 6, 0);   /* checksum 0 = none (legal for IPv4) */
    return ip_out(n, 17, src, dst, 8 + plen, dmac);
}

/* ---------- DHCP ---------- */
static void dhcp_send(Net *n, int type) {                                  /* 1 DISCOVER, 3 REQUEST */
    uint8_t *d = n->fb + PAY + 8; nset(d, 0, 240); int o = 240;
    d[0] = 1; d[1] = 1; d[2] = 6; nwr32(d + 4, n->xid); nwr16(d + 10, 0x8000); ncpy(d + 28, n->mac, 6); nwr32(d + 236, 0x63825363u);
    d[o++] = 53; d[o++] = 1; d[o++] = (uint8_t)type;
    d[o++] = 61; d[o++] = 7; d[o++] = 1; ncpy(d + o, n->mac, 6); o += 6;
    if (type == 3) { d[o++] = 50; d[o++] = 4; nwr32(d + o, n->offered); o += 4; d[o++] = 54; d[o++] = 4; nwr32(d + o, n->dhcp_srv); o += 4; }
    d[o++] = 55; d[o++] = 3; d[o++] = 1; d[o++] = 3; d[o++] = 6; d[o++] = 255;
    while (o < 300) d[o++] = 0;
    udp_out(n, 0, 0xffffffffu, 68, 67, o, NBCAST);
}
static void dhcp_rx(Net *n, const uint8_t *d, int len) {
    if (len < 241 || d[0] != 2 || nrd32(d + 4) != n->xid || ncmp(d + 28, n->mac, 6) || nrd32(d + 236) != 0x63825363u) return;
    int type = 0; uint32_t mask = 0, gw = 0, dns = 0, srv = 0;
    for (int o = 240; o < len;) {
        int c = d[o++]; if (c == 255) break; if (c == 0) continue; if (o >= len) break; int l = d[o++]; if (o + l > len) break;
        if (c == 53 && l >= 1) type = d[o]; else if (c == 1 && l >= 4) mask = nrd32(d + o); else if (c == 3 && l >= 4) gw = nrd32(d + o);
        else if (c == 6 && l >= 4) dns = nrd32(d + o); else if (c == 54 && l >= 4) srv = nrd32(d + o);
        o += l;
    }
    if (type == 2 && n->state == NS_DHCP && !n->offered) { n->offered = nrd32(d + 16); n->dhcp_srv = srv; n->t_state = NNOW(n); n->retries = 0; dhcp_send(n, 3); }
    else if (type == 5 && n->state == NS_DHCP && n->offered) {
        n->ip = nrd32(d + 16); n->mask = mask ? mask : 0xffffff00u; n->gw = gw; n->dns = dns ? dns : gw; n->state = NS_ARP; n->t_state = 0; n->retries = 0;
    } else if (type == 6) { n->offered = 0; }
}

/* ---------- DNS ---------- */
static void dns_send(Net *n) {
    uint8_t *q = n->fb + PAY + 8; int o = 12; const char *s = n->dns_name;
    nset(q, 0, 12); nwr16(q, n->dns_id); nwr16(q + 2, 0x0100); nwr16(q + 4, 1);
    while (*s) { int l = 0; while (s[l] && s[l] != '.') l++; if (l == 0 || l > 63) { n->dns_busy = 0; return; } q[o++] = (uint8_t)l; ncpy(q + o, s, l); o += l; s += l; if (*s == '.') s++; }
    q[o++] = 0; nwr16(q + o, 1); nwr16(q + o + 2, 1); o += 4;
    udp_out(n, n->ip, n->dns, n->dns_port, 53, o, 0);
}
static int dns_skip_name(const uint8_t *d, int len, int o) {
    while (o < len) { int c = d[o]; if (c == 0) return o + 1; if ((c & 0xC0) == 0xC0) return o + 2; o += 1 + c; }
    return -1;
}
static void dns_rx(Net *n, const uint8_t *d, int len) {
    if (!n->dns_busy || len < 12 || nrd16(d) != n->dns_id || !(d[2] & 0x80)) return;
    if ((d[3] & 0x0f) != 0) { n->dns_busy = 0; n->dns_result = 0; n->dns_tries = 99; return; }
    int qd = nrd16(d + 4), an = nrd16(d + 6), o = 12;
    for (int i = 0; i < qd; i++) { o = dns_skip_name(d, len, o); if (o < 0) return; o += 4; }
    for (int i = 0; i < an; i++) {
        o = dns_skip_name(d, len, o); if (o < 0 || o + 10 > len) return;
        int type = nrd16(d + o), cls = nrd16(d + o + 2), rl = nrd16(d + o + 8); o += 10; if (o + rl > len) return;
        if (type == 1 && cls == 1 && rl == 4) { n->dns_result = nrd32(d + o); n->dns_busy = 0; return; }
        o += rl;
    }
}
int net_dns(Net *n, const char *name, uint32_t *ip) {
    if (name) {
        uint32_t lit = net_parse_ip(name); if (lit) { *ip = lit; n->dns_result = lit; n->dns_busy = 0; return 1; }
        if (nlen(name) >= (int)sizeof n->dns_name) return -1;
        if (n->state != NS_READY) return -1;
        ncpy(n->dns_name, name, nlen(name) + 1); n->dns_busy = 1; n->dns_result = 0; n->dns_tries = 0; n->dns_t = NNOW(n) - 5000u;
        { uint8_t r[2]; n->ops.rnd(n->ops.ctx, r, 2); n->dns_id = nrd16(r); n->dns_port = (uint16_t)(49152 + nrd16(r) % 9000); }
        return 0;
    }
    if (n->dns_busy) return 0;
    if (n->dns_result) { *ip = n->dns_result; return 1; }
    return -1;
}

/* ---------- TCP ---------- */
#define TF_FIN 1
#define TF_SYN 2
#define TF_RST 4
#define TF_PSH 8
#define TF_ACK 16
static int tcp_free(Net *n) { return NET_RX_MAX - n->tcp.rcount; }
static int tcp_send(Net *n, int flags, const uint8_t *data, int dlen, uint32_t seq) {
    uint8_t *t = n->fb + PAY; int hl = (flags & TF_SYN) ? 24 : 20; int w = tcp_free(n); if (w > 65535) w = 65535;
    nwr16(t, n->tcp.lport); nwr16(t + 2, n->tcp.rport); nwr32(t + 4, seq); nwr32(t + 8, (flags & TF_ACK) ? n->tcp.rcv_nxt : 0);
    t[12] = (uint8_t)((hl / 4) << 4); t[13] = (uint8_t)flags; nwr16(t + 14, (uint32_t)w); nwr16(t + 16, 0); nwr16(t + 18, 0);
    if (flags & TF_SYN) { t[20] = 2; t[21] = 4; nwr16(t + 22, NET_ADV_MSS); }
    if (dlen) ncpy(t + hl, data, dlen);
    { uint8_t ph[12]; nwr32(ph, n->ip); nwr32(ph + 4, n->tcp.rip); ph[8] = 0; ph[9] = 6; nwr16(ph + 10, (uint32_t)(hl + dlen));
      nwr16(t + 16, ncsum_fin(ncsum_add(ncsum_add(0, ph, 12), t, hl + dlen))); }
    return ip_out(n, 6, n->ip, n->tcp.rip, hl + dlen, 0);
}
static void tcp_seg_next(Net *n) {                                         /* send (or resend) the segment at the head of the unsent queue */
    int rem = n->tcp.txlen - n->tcp.txoff; int l = rem < n->tcp.mss ? rem : n->tcp.mss;
    n->tcp.seglen = l; n->tcp.snd_nxt = n->tcp.snd_una + (uint32_t)l; n->tcp.t0 = NNOW(n); n->tcp.tries = 0;
    tcp_send(n, TF_ACK | TF_PSH, n->tcp.tx + n->tcp.txoff, l, n->tcp.snd_una);
}
int net_tcp_connect(Net *n, uint32_t ip, uint16_t port) {
    uint8_t r[6]; if (n->state != NS_READY) return -1;
    n->ops.rnd(n->ops.ctx, r, 6);
    n->tcp.state = TCP_SYN; n->tcp.rip = ip; n->tcp.rport = port; n->tcp.lport = (uint16_t)(40000 + nrd16(r) % 20000); n->tcp.iss = nrd32(r + 2);
    n->tcp.snd_una = n->tcp.iss; n->tcp.snd_nxt = n->tcp.iss + 1; n->tcp.rcv_nxt = 0; n->tcp.peer_fin = 0; n->tcp.tx_busy = 0; n->tcp.txlen = 0; n->tcp.txoff = 0;
    n->tcp.rhead = 0; n->tcp.rcount = 0; n->tcp.mss = 536; n->tcp.tries = 0; n->tcp.t0 = NNOW(n);
    tcp_send(n, TF_SYN, 0, 0, n->tcp.iss);
    return 0;
}
int net_tcp_write(Net *n, const uint8_t *d, int len) {
    if (n->tcp.state != TCP_EST || n->tcp.tx_busy || len <= 0) return 0;
    if (len > NET_TX_MAX) len = NET_TX_MAX;
    ncpy(n->tcp.tx, d, len); n->tcp.txlen = len; n->tcp.txoff = 0; n->tcp.tx_busy = 1; tcp_seg_next(n);
    return len;
}
int net_tcp_read(Net *n, uint8_t *d, int max) {
    int c = n->tcp.rcount < max ? n->tcp.rcount : max;
    for (int i = 0; i < c; i++) d[i] = n->tcp.rx[(n->tcp.rhead + i) % NET_RX_MAX];
    n->tcp.rhead = (n->tcp.rhead + c) % NET_RX_MAX; n->tcp.rcount -= c;
    return c;
}
void net_tcp_close(Net *n) {
    if (n->tcp.state == TCP_EST && !n->tcp.tx_busy) { tcp_send(n, TF_FIN | TF_ACK, 0, 0, n->tcp.snd_nxt); n->tcp.snd_nxt++; n->tcp.state = TCP_FINW; n->tcp.t0 = NNOW(n); }
    else if (n->tcp.state != TCP_FINW) n->tcp.state = TCP_CLOSED;
}
static void tcp_rx(Net *n, uint32_t sip, const uint8_t *s, int len) {
    if (len < 20 || n->tcp.state == TCP_CLOSED || sip != n->tcp.rip) return;
    uint16_t sp = nrd16(s), dp = nrd16(s + 2); uint32_t seq = nrd32(s + 4), ack = nrd32(s + 8); int hl = (s[12] >> 4) * 4, fl = s[13];
    if (sp != n->tcp.rport || dp != n->tcp.lport || hl < 20 || hl > len) return;
    if (fl & TF_RST) { n->tcp.state = TCP_ERR; return; }
    const uint8_t *pl = s + hl; int plen = len - hl;
    if (n->tcp.state == TCP_SYN) {
        if ((fl & (TF_SYN | TF_ACK)) == (TF_SYN | TF_ACK) && ack == n->tcp.iss + 1) {
            for (int o = 20; o + 1 < hl;) { int k = s[o]; if (k == 0) break; if (k == 1) { o++; continue; } int l = s[o + 1]; if (l < 2) break; if (k == 2 && l == 4 && o + 4 <= hl) { int m = nrd16(s + o + 2); if (m > 0 && m < 1200) n->tcp.mss = (uint16_t)m; else if (m >= 1200) n->tcp.mss = 1200; } o += l; }
            n->tcp.rcv_nxt = seq + 1; n->tcp.snd_una = ack; n->tcp.snd_nxt = ack; n->tcp.state = TCP_EST; tcp_send(n, TF_ACK, 0, 0, n->tcp.snd_nxt);
        }
        return;
    }
    if (fl & TF_ACK) {
        if (n->tcp.tx_busy && ack == n->tcp.snd_nxt && (int32_t)(ack - n->tcp.snd_una) > 0) {
            n->tcp.snd_una = ack; n->tcp.txoff += n->tcp.seglen;
            if (n->tcp.txoff < n->tcp.txlen) tcp_seg_next(n); else n->tcp.tx_busy = 0;
        }
    }
    if (plen > 0 || (fl & TF_FIN)) {
        if (seq == n->tcp.rcv_nxt && plen > 0) {
            int space = tcp_free(n), c = plen < space ? plen : space;
            for (int i = 0; i < c; i++) n->tcp.rx[(n->tcp.rhead + n->tcp.rcount + i) % NET_RX_MAX] = pl[i];
            n->tcp.rcount += c; n->tcp.rcv_nxt += (uint32_t)c;
            if (c == plen && (fl & TF_FIN)) { n->tcp.rcv_nxt++; n->tcp.peer_fin = 1; }
        } else if (seq == n->tcp.rcv_nxt && plen == 0 && (fl & TF_FIN)) { n->tcp.rcv_nxt++; n->tcp.peer_fin = 1; }
        tcp_send(n, TF_ACK, 0, 0, n->tcp.snd_nxt);                              /* also answers out-of-order data with a duplicate ACK */
    }
}

/* ---------- IP receive ---------- */
static void ip_rx(Net *n, const uint8_t *p, int len) {
    if (len < 20 || (p[0] >> 4) != 4) return;
    int ihl = (p[0] & 15) * 4, tot = nrd16(p + 2); if (ihl < 20 || tot > len || tot < ihl) return;
    if (nrd16(p + 6) & 0x3fff) return;                                          /* fragments are not supported */
    uint32_t src = nrd32(p + 12), dst = nrd32(p + 16); int proto = p[9];
    const uint8_t *pl = p + ihl; int plen = tot - ihl;
    if (n->ip && dst != n->ip && dst != 0xffffffffu && (dst | ~n->mask) != dst) return;
    if (proto == 17 && plen >= 8) {
        int dp = nrd16(pl + 2), sp = nrd16(pl); (void)sp; int ul = nrd16(pl + 4); if (ul > plen) ul = plen; if (ul < 8) return;
        if (dp == 68) dhcp_rx(n, pl + 8, ul - 8);
        else if (n->dns_busy && dp == n->dns_port) dns_rx(n, pl + 8, ul - 8);
    } else if (proto == 6) { if (n->ip && dst == n->ip) tcp_rx(n, src, pl, plen); }
    else if (proto == 1 && plen >= 8 && pl[0] == 8 && n->ip && dst == n->ip && plen <= 1400) {   /* ICMP echo -> reply (handy for debugging) */
        ncpy(n->fb + PAY, pl, plen); n->fb[PAY] = 0; nwr16(n->fb + PAY + 2, 0); nwr16(n->fb + PAY + 2, ncsum_fin(ncsum_add(0, n->fb + PAY, plen)));
        ip_out(n, 1, n->ip, src, plen, 0);
    }
}

/* ---------- BNEP receive ---------- */
static void bnep_ctl(Net *n, const uint8_t *m, int len) {
    if (len < 1) return;
    switch (m[0]) {
    case 0x02: if (len >= 3 && n->state == NS_BNEP) { if (nrd16(m + 1) == 0) { n->bnep_ok = 1; n->state = NS_DHCP; n->offered = 0; n->retries = 0; n->t_state = NNOW(n) - 5000u; } else n->state = NS_FAIL; } break;
    case 0x03: bnep_ctl_reply(n, 0x04, 0); break;            /* NET_TYPE_FILTER_SET_MSG -> success */
    case 0x05: bnep_ctl_reply(n, 0x06, 0); break;            /* MULTICAST_FILTER_SET_MSG -> success */
    case 0x01: bnep_ctl_reply(n, 0x02, 0x0001); break;       /* a SETUP request from the NAP: refuse (we are the client) */
    case 0x00: case 0x04: case 0x06: break;
    default: { uint8_t r[3] = {0x01, 0x00, m[0]}; n->ops.tx(n->ops.ctx, r, 3); }
    }
}
void net_link_rx(Net *n, const uint8_t *p, int len) {
    if (len < 1) return;
    int type = p[0] & 0x7f, ext = p[0] & 0x80, o; const uint8_t *src = 0; uint16_t proto;
    if (type == 0x01) { bnep_ctl(n, p + 1, len - 1); return; }
    if (type == 0x00) { if (len < 15) return; src = p + 7; proto = nrd16(p + 13); o = 15; }
    else if (type == 0x02) { if (len < 3) return; proto = nrd16(p + 1); o = 3; }
    else if (type == 0x03) { if (len < 9) return; src = p + 1; proto = nrd16(p + 7); o = 9; }
    else if (type == 0x04) { if (len < 9) return; proto = nrd16(p + 7); o = 9; }
    else return;
    while (ext) { if (o + 2 > len) return; ext = p[o] & 0x80; o += 2 + p[o + 1]; }
    if (o > len) return;
    if (src) { ncpy(n->srv_mac, src, 6); n->have_srv_mac = 1; }
    if (proto == 0x0806) arp_rx(n, p + o, len - o); else if (proto == 0x0800) ip_rx(n, p + o, len - o);
}

#ifndef NET_NO_SC
/* ---------- secure channel ---------- */
static void sc_nonce(uint8_t nonce[12], int dir, uint64_t ctr) { nonce[0] = (uint8_t)dir; nonce[1] = nonce[2] = nonce[3] = 0; for (int i = 0; i < 8; i++) nonce[4 + i] = (uint8_t)(ctr >> (56 - 8 * i)); }
int sc_connect(Net *n, const char *host, uint16_t port, const uint8_t psk[32]) {
    if (nlen(host) >= (int)sizeof n->sc.host) return -1;
    ncpy(n->sc.psk, psk, 32); ncpy(n->sc.host, host, nlen(host) + 1); n->sc.port = port; n->sc.sctr = n->sc.rctr = 0; n->sc.sent_hello = n->sc.got_hello = 0;
    n->tcp.state = TCP_CLOSED; n->sc.state = SC_DNS; { uint32_t ip; int r = net_dns(n, host, &ip); if (r < 0) { n->sc.state = SC_ERR; return -1; } }
    return 0;
}
void sc_close(Net *n) { net_tcp_close(n); n->sc.state = SC_IDLE; }
static int sc_peek(Net *n, uint8_t *d, int c) { if (n->tcp.rcount < c) return 0; for (int i = 0; i < c; i++) d[i] = n->tcp.rx[(n->tcp.rhead + i) % NET_RX_MAX]; return 1; }
static int rec_read(Net *n, uint8_t *out, int max) {                       /* one decrypted record: >0 length (0-length records return 1 via flag), 0 none, -1 error */
    uint8_t h[2]; if (!sc_peek(n, h, 2)) return 0;
    int cl = nrd16(h); if (cl < 16 || cl > SC_MSG_MAX + 16) return -1;
    if (n->tcp.rcount < 2 + cl) return 0;
    net_tcp_read(n, h, 2); net_tcp_read(n, n->sc.rxbuf, cl);
    { uint8_t nonce[12]; int pl = cl - 16; if (pl > max) return -1; sc_nonce(nonce, 1, n->sc.rctr);
      if (aead_open(n->sc.key, nonce, h, 2, n->sc.rxbuf, pl, out) != 0) return -1;
      n->sc.rctr++; return pl; }
}
static void sc_step(Net *n) {
    switch (n->sc.state) {
    case SC_DNS: { uint32_t ip; int r = net_dns(n, 0, &ip); if (r == 1) { if (net_tcp_connect(n, ip, n->sc.port) == 0) n->sc.state = SC_TCP; else n->sc.state = SC_ERR; } else if (r < 0) n->sc.state = SC_ERR; break; }
    case SC_TCP:
        if (n->tcp.state == TCP_EST) { n->ops.rnd(n->ops.ctx, n->sc.cn, 8); if (net_tcp_write(n, n->sc.cn, 8) == 8) n->sc.state = SC_HS; }
        else if (n->tcp.state == TCP_ERR || n->tcp.state == TCP_CLOSED) n->sc.state = SC_ERR;
        break;
    case SC_HS:
        if (n->tcp.state == TCP_ERR) { n->sc.state = SC_ERR; break; }
        if (!n->sc.sent_hello) {                                              /* "sent_hello" here means: keys derived */
            if (n->tcp.rcount < 8) break;
            { uint8_t nonce[12], blk[64], k1[32]; net_tcp_read(n, n->sc.sn, 8);
              nset(nonce, 0, 12); ncpy(nonce, n->sc.cn, 8); cc20_block(n->sc.psk, 0, nonce, blk); ncpy(k1, blk, 32);
              nset(nonce, 0, 12); ncpy(nonce, n->sc.sn, 8); cc20_block(k1, 1, nonce, blk); ncpy(n->sc.key, blk, 32); }
            n->sc.sent_hello = 1;
        }
        { uint8_t tmp[8]; int r = rec_read(n, tmp, 8); if (r == 2 && tmp[0] == 'O' && tmp[1] == 'K') n->sc.state = SC_READY; else if (r < 0 || r > 0) n->sc.state = SC_ERR; }
        break;
    case SC_READY: if (n->tcp.state == TCP_ERR) n->sc.state = SC_ERR; break;
    default: break;
    }
}
int sc_send(Net *n, const uint8_t *msg, int len) {
    if (n->sc.state != SC_READY || len <= 0 || len > SC_MSG_MAX || n->tcp.tx_busy) return -1;
    uint8_t *rec = n->sc.rxbuf, nonce[12]; nwr16(rec, (uint32_t)(len + 16));     /* rxbuf (SC_MSG_MAX+40) doubles as scratch: keeps the firmware stack small */ sc_nonce(nonce, 0, n->sc.sctr);
    aead_seal(n->sc.key, nonce, rec, 2, msg, len, rec + 2); n->sc.sctr++;
    return net_tcp_write(n, rec, len + 18) == len + 18 ? 0 : -1;
}
int sc_recv(Net *n, uint8_t *out, int max) {
    if (n->sc.state != SC_READY) return n->sc.state == SC_ERR ? -1 : 0;
    { int r = rec_read(n, out, max); if (r < 0) { n->sc.state = SC_ERR; return -1; } return r; }
}

#endif
/* ---------- lifecycle / timers ---------- */
void net_init(Net *n, const NetOps *ops, const uint8_t mac[6]) {
    nset(n, 0, (int)sizeof *n); n->ops = *ops; ncpy(n->mac, mac, 6); n->state = NS_DOWN;
}
void net_link_up(Net *n) {
    uint8_t r[4]; n->ops.rnd(n->ops.ctx, r, 4); n->xid = nrd32(r);
    n->ip = n->mask = n->gw = n->dns = 0; n->offered = 0; for (int i = 0; i < 4; i++) n->arp[i].ok = 0;
    n->state = NS_BNEP; n->retries = 0; n->t_state = NNOW(n); bnep_setup_req(n);
}
void net_link_down(Net *n) { n->state = NS_DOWN; n->tcp.state = TCP_CLOSED; n->sc.state = SC_IDLE; n->dns_busy = 0; }
void net_tick(Net *n) {
    uint32_t now = NNOW(n);
    switch (n->state) {
    case NS_BNEP: if (now - n->t_state > 3000u) { if (++n->retries > 3) n->state = NS_FAIL; else { n->t_state = now; bnep_setup_req(n); } } break;
    case NS_DHCP: if (now - n->t_state > 2500u) { if (++n->retries > 6) { n->state = NS_FAIL; break; } n->t_state = now; dhcp_send(n, n->offered ? 3 : 1); } break;
    case NS_ARP:
        if (!n->gw || arp_find(n, n->gw)) { n->state = NS_READY; break; }
        if (n->t_state == 0 || now - n->t_state > 1000u) { if (++n->retries > 8) { n->state = NS_FAIL; break; } n->t_state = now ? now : 1; arp_request(n, n->gw); }
        break;
    case NS_READY:
        if (n->dns_busy && now - n->dns_t > 1500u) { if (++n->dns_tries > 4) { n->dns_busy = 0; n->dns_result = 0; } else { n->dns_t = now; dns_send(n); } }
        if (n->tcp.state == TCP_SYN && now - n->tcp.t0 > 1500u) { if (++n->tcp.tries > 5) n->tcp.state = TCP_ERR; else { n->tcp.t0 = now; tcp_send(n, TF_SYN, 0, 0, n->tcp.iss); } }
        if (n->tcp.tx_busy && now - n->tcp.t0 > 1500u) { if (++n->tcp.tries > 8) { n->tcp.state = TCP_ERR; n->tcp.tx_busy = 0; } else { n->tcp.t0 = now; { int rem = n->tcp.txlen - n->tcp.txoff; int l = rem < n->tcp.mss ? rem : n->tcp.mss; tcp_send(n, TF_ACK | TF_PSH, n->tcp.tx + n->tcp.txoff, l, n->tcp.snd_una); } } }
        if (n->tcp.state == TCP_FINW && now - n->tcp.t0 > 3000u) n->tcp.state = TCP_CLOSED;
#ifndef NET_NO_SC
        sc_step(n);
#endif
        break;
    default: break;
    }
}
