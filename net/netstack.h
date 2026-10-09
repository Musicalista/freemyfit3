/* Minimal network stack for the Fit3 over Bluetooth PAN (BNEP, PANU role): BNEP + Ethernet + ARP + IPv4 + ICMP echo + UDP + DHCP + DNS + TCP (client)
 * + an encrypted request/response channel (ChaCha20-Poly1305) to a private proxy. Portable C, no libc, no globals (state lives in Net).
 * The Bluetooth side is behind NetOps: tx() sends one L2CAP SDU (a BNEP frame); the caller feeds received SDUs to net_link_rx() and calls net_tick() every ~50 ms. */
#ifndef NETSTACK_H
#define NETSTACK_H
#include <stdint.h>

#define NET_RX_MAX 4096        /* TCP receive ring */
#define NET_TX_MAX 1300        /* one TCP segment in flight (stop-and-wait) */
#define SC_MSG_MAX 1100
#define NET_ADV_MSS 536        /* MSS we announce: keeps every frame we receive (<= 591 bytes) inside any firmware buffer */        /* one secure-channel record carries up to this many plaintext bytes */

enum { NS_DOWN, NS_BNEP, NS_DHCP, NS_ARP, NS_READY, NS_FAIL };
enum { TCP_CLOSED, TCP_SYN, TCP_EST, TCP_FINW, TCP_ERR };
enum { SC_IDLE, SC_DNS, SC_TCP, SC_HS, SC_READY, SC_ERR };

typedef struct {
    void (*tx)(void *ctx, const uint8_t *sdu, int len);       /* send one BNEP frame as an L2CAP SDU */
    uint32_t (*now)(void *ctx);                               /* milliseconds */
    void (*rnd)(void *ctx, uint8_t *out, int n);              /* random bytes (xid, ports, nonces) */
    void *ctx;
} NetOps;

typedef struct { uint32_t ip; uint8_t mac[6]; uint8_t ok; } ArpEnt;

typedef struct Net {
    NetOps ops; uint8_t mac[6];
    uint8_t state, bnep_ok; uint32_t t_state; int retries;
    /* addressing */
    uint32_t ip, mask, gw, dns, dhcp_srv, offered; uint32_t xid; uint8_t srv_mac[6]; uint8_t have_srv_mac;
    ArpEnt arp[4]; uint8_t arp_next; uint32_t arp_t; uint32_t arp_want;
    /* DNS (one query at a time) */
    uint8_t dns_busy; uint16_t dns_id, dns_port; uint32_t dns_result, dns_t; uint8_t dns_tries; char dns_name[64];
    /* TCP (one connection) */
    struct {
        uint8_t state, peer_fin, tx_busy, tries; uint16_t lport, rport, mss; uint32_t rip, iss, snd_una, snd_nxt, rcv_nxt, t0; int txlen, txoff, seglen;
        uint8_t tx[NET_TX_MAX]; uint8_t rx[NET_RX_MAX]; int rhead, rcount;
    } tcp;
    /* secure channel */
    struct { uint8_t state, key[32], psk[32], cn[8], sn[8]; uint32_t ip; uint16_t port; uint64_t sctr, rctr; char host[64]; uint8_t sent_hello, got_hello; uint8_t rxbuf[SC_MSG_MAX + 40]; } sc;
    uint16_t ipid;
    uint8_t fb[1700];                                          /* frame under construction */
} Net;

void net_init(Net *n, const NetOps *ops, const uint8_t mac[6]);
void net_link_up(Net *n);                                      /* L2CAP channel to the NAP is open: start the BNEP handshake */
void net_link_down(Net *n);
void net_link_rx(Net *n, const uint8_t *sdu, int len);         /* one received BNEP frame */
void net_tick(Net *n);                                         /* timers: DHCP/ARP/DNS/TCP retransmits, secure-channel progress */

/* DNS: returns 1 when the answer is ready (and sets *ip), 0 while pending, -1 on failure. Pass NULL name to poll. */
int net_dns(Net *n, const char *name, uint32_t *ip);
/* TCP client: one connection. */
int net_tcp_connect(Net *n, uint32_t ip, uint16_t port);       /* 0 started, -1 not ready */
int net_tcp_write(Net *n, const uint8_t *d, int len);          /* bytes accepted (0 = busy: one segment in flight) */
int net_tcp_read(Net *n, uint8_t *d, int max);                 /* bytes copied from the receive ring */
void net_tcp_close(Net *n);
/* secure channel to the proxy: host is a name or dotted IPv4; psk is the 32-byte pre-shared key. */
int sc_connect(Net *n, const char *host, uint16_t port, const uint8_t psk[32]);
int sc_send(Net *n, const uint8_t *msg, int len);              /* 0 ok, -1 busy/not ready */
int sc_recv(Net *n, uint8_t *out, int max);                    /* >0 one decrypted message, 0 none yet, -1 error */
void sc_close(Net *n);

uint32_t net_parse_ip(const char *s);                          /* dotted IPv4 -> host-order u32, 0 if not an IP literal */
#endif
