/* ================= Direct web access over the Bluetooth tethering (reader netmode 4) =================
 * No proxy and no app on the phone: the watch resolves the name (DNS), opens the TCP connection, speaks TLS 1.3 itself (net/tlsc.c; the certificate is NOT verified)
 * and HTTP/1.1, and converts the HTML to the reader's text page on the fly (net/webc.c). Included by net.inc.c just before ng_tick(). */
enum { DS_IDLE, DS_DNS, DS_CONNECT, DS_TLS, DS_SEND, DS_RECV };

static void *dir_alloc(uint32_t n) { return MALLOC((u32)n); }
static void dir_free(void *p) { FREE(p); }
static void dir_rnd(void *ctx, u8 *o, int n) { ng_rnd(ctx, o, n); }
static void dir_cleanup(NG *g) {
    if (g->tls) { tls_free(g->tls, dir_free); g->tls = 0; }
    if (g->web) { FREE(g->web); g->web = 0; }
    net_tcp_close(g->net); g->ds = DS_IDLE;
}
static void dir_fail(WR *w, const char *pt, const char *en) {
    NG *g = (NG *)w->ng; dir_cleanup(g); g->have_pending = 0; g->resp_on = 0; ng_page_error(w, pt, en); w->mode = WM_PAGE; w->dirty = 1; MOTOR_ONCE(5, 0);
}
static void dir_start_url(WR *w, const char *url) {                               /* (re)start the transfer of one URL */
    NG *g = (NG *)w->ng; Web *wb = g->web;
    for (int i = 0; i < (int)sizeof wb->url - 1 && url[i]; i++) { wb->url[i] = url[i]; wb->url[i + 1] = 0; }
    if (web_parse_url(wb, wb->url)) { dir_fail(w, "Endereco invalido (use http:// ou https://).", "Invalid address (use http:// or https://)."); return; }
    web_reset_response(wb); g->ds = DS_DNS; g->dns_go = 0; g->dt0 = TICK_GET(); g->dreq_off = 0; g->dreq_n = 0;
}
static void dir_begin(WR *w) {                                                    /* g->pending holds the address or search words */
    NG *g = (NG *)w->ng; Web *wb = (Web *)MALLOC(sizeof(Web));
    if (!wb) { dir_fail(w, "Sem memoria para abrir a pagina.", "Not enough memory to open the page."); return; }
    for (unsigned i = 0; i < sizeof *wb; i++) ((u8 *)wb)[i] = 0;
    g->web = wb; g->hops = 0; g->have_pending = 0; g->resp_on = 1; g->resp_len = 0; g->req_t0 = TICK_GET();
    char url[300]; const char *q = g->pending; int n = 0;
    if (web_looks_like_url(q)) { if (!web_has_scheme(q)) { const char *pre = "https://"; while (*pre) url[n++] = *pre++; } while (*q && n < 290) url[n++] = *q++; url[n] = 0; }
    else web_search_url(q, url, (int)sizeof url);
    dir_start_url(w, url);
}
static void dir_done(WR *w) {
    NG *g = (NG *)w->ng; Web *wb = g->web; int n = web_page(wb, (char *)w->buf, WR_BUF - 8, 1);
    w->len = n; w->buf[n] = 0; dir_cleanup(g); g->resp_on = 0;
    if (wr_parse(w, 0)) { w->mode = WM_PAGE; MOTOR_ONCE(1, 0); } else ng_page_error(w, "Pagina invalida.", "Invalid page.");
    w->dirty = 1;
}
static int dir_pump_out(NG *g) {                                                  /* move queued TLS bytes to TCP; 1 if something moved */
    Tls *t = g->tls; int moved = 0;
    while (t->out_len > 0) { int k = t->out_len > 1200 ? 1200 : t->out_len; int n = net_tcp_write(g->net, t->out, k); if (n <= 0) break; tls_out_consumed(t, n); moved = 1; if (n < k) break; }
    return moved;
}
static void dir_net_result(WR *w, int r) {                                        /* r from web_feed / web_eof */
    NG *g = (NG *)w->ng; Web *wb = g->web;
    if (r == 1) { dir_done(w); }
    else if (r == 2) {
        char nu[WB_URL + 100]; if (++g->hops > 5 || web_resolve(wb->url, wb->loc, nu, (int)sizeof nu) < 0) { dir_fail(w, "Redirecionamentos demais.", "Too many redirects."); return; }
        if (g->tls) { tls_free(g->tls, dir_free); g->tls = 0; } net_tcp_close(g->net); dir_start_url(w, nu);
    } else if (r < 0) {
        if (wb->enc_bad) dir_fail(w, "O site respondeu comprimido (gzip); ainda nao suportado.", "The site answered compressed (gzip); not supported yet.");
        else if (wb->bin) dir_fail(w, "Este tipo de arquivo nao pode ser mostrado no relogio.", "This kind of file cannot be shown on the watch.");
        else dir_fail(w, "O site fechou a conexao sem responder.", "The site closed the connection without answering.");
    }
}
static void dir_tick(WR *w) {
    NG *g = (NG *)w->ng; Net *net = g->net; u32 now = TICK_GET();
    if (!g->welcomed && !g->have_pending && g->ds == DS_IDLE && !g->resp_on) {
        g->welcomed = 1;
        wr_set_text(w, TR("Internet", "Internet"), TR("Conectado pela ancoragem Bluetooth do celular. Nao precisa de mais nada: o relogio acessa os sites sozinho.\n\nToque em Ir, digite um endereco (ex.: example.com) ou palavras para buscar e confirme com OK.\n\nToque em um [n] para abrir um link. Arraste para rolar. L lista os links; < volta.\n\nAtencao: o certificado dos sites HTTPS nao e verificado (a conexao e cifrada, mas nao e provado que o site e quem diz ser). Nao digite senhas.",
            "Connected through your phone's Bluetooth tethering. Nothing else is needed: the watch reaches websites by itself.\n\nTap Go, type an address (e.g. example.com) or search words and confirm with OK.\n\nTap a [n] to open a link. Drag to scroll. L lists the links; < goes back.\n\nNote: HTTPS certificates are NOT verified (the connection is encrypted, but the site is not proven to be who it claims). Do not type passwords."));
        w->mode = WM_PAGE; MOTOR_ONCE(1, 0); w->dirty = 1; return;
    }
    if (g->ds == DS_IDLE) { if (g->have_pending) dir_begin(w); return; }
    Web *wb = g->web;
    if (now - g->dt0 > 40000u) { dir_fail(w, "A conexao demorou demais (40 s).", "The connection took too long (40 s)."); return; }
    switch (g->ds) {
    case DS_DNS: {
        uint32_t ip = net_parse_ip(wb->host); int r;
        if (ip) r = 1; else if (!g->dns_go) { r = net_dns(net, wb->host, &ip); g->dns_go = 1; } else r = net_dns(net, 0, &ip);
        if (r == 1) { if (net_tcp_connect(net, ip, wb->port) == 0) { g->ds = DS_CONNECT; g->dt0 = now; } else dir_fail(w, "Rede ocupada, tente de novo.", "Network busy, try again."); }
        else if (r < 0) dir_fail(w, "Nao foi possivel achar o endereco do site (DNS).", "Could not find the website address (DNS).");
        break; }
    case DS_CONNECT:
        if (net->tcp.state == TCP_EST) {
            g->dt0 = now;
            if (wb->https) {
                g->tls = tls_new(dir_alloc, dir_free); if (!g->tls) { dir_fail(w, "Sem memoria para o TLS.", "Not enough memory for TLS."); return; }
                tls_start(g->tls, wb->host, dir_rnd, g); g->ds = DS_TLS;
            } else { g->dreq_n = web_request(wb, g->dreq, (int)sizeof g->dreq); g->dreq_off = 0; g->ds = DS_SEND; }
        } else if (net->tcp.state == TCP_ERR || net->tcp.state == TCP_CLOSED) dir_fail(w, "O site nao aceitou a conexao.", "The site refused the connection.");
        break;
    case DS_TLS: {
        Tls *t = g->tls; int guard = 0;
        while (guard++ < 6) {
            int moved = dir_pump_out(g);
            if (t->state == TLS_ERR) { dir_fail(w, "Falha no TLS (o site pode exigir outra versao).", "TLS failure (the site may need another version)."); return; }
            if (t->state == TLS_APP) { g->dreq_n = web_request(wb, g->dreq, (int)sizeof g->dreq); if (tls_send(t, (const u8 *)g->dreq, g->dreq_n) < 0) { dir_fail(w, "Pedido grande demais.", "Request too large."); return; } dir_pump_out(g); g->ds = DS_RECV; g->dt0 = now; break; }
            int want = tls_want(t); u8 b[600]; if (want > (int)sizeof b) want = (int)sizeof b;
            int n = want > 0 ? net_tcp_read(net, b, want) : 0; if (n > 0) { tls_rx(t, b, n); g->dt0 = now; moved = 1; }
            if (!moved) break;
        }
        if (net->tcp.state != TCP_EST && net->tcp.state != TCP_FINW && g->ds == DS_TLS) dir_fail(w, "A conexao caiu durante o TLS.", "The connection dropped during TLS.");
        break; }
    case DS_SEND:
        while (g->dreq_off < g->dreq_n) { int k = g->dreq_n - g->dreq_off; if (k > 1200) k = 1200; int n = net_tcp_write(net, (const u8 *)g->dreq + g->dreq_off, k); if (n <= 0) break; g->dreq_off += n; }
        if (g->dreq_off >= g->dreq_n) { g->ds = DS_RECV; g->dt0 = now; }
        break;
    case DS_RECV: {
        int budget = 6, r = 0; u8 b[600];
        while (budget-- > 0 && !r) {
            if (g->tls) {
                Tls *t = g->tls; dir_pump_out(g); int k;
                while (!r && (k = tls_read(t, b, sizeof b)) > 0) { g->dt0 = now; g->resp_len += k; r = web_feed(wb, b, k); }
                if (r) break;
                if (t->state == TLS_ERR) { r = web_eof(wb); if (r == -1) { dir_fail(w, "A conexao segura foi interrompida.", "The secure connection was interrupted."); return; } break; }
                if (t->state == TLS_CLOSED && tls_want(t) == 0) { r = web_eof(wb); if (!r) r = -1; break; }
                int want = tls_want(t); if (want > (int)sizeof b) want = (int)sizeof b;
                int n = want > 0 ? net_tcp_read(net, b, want) : 0; if (n > 0) { tls_rx(t, b, n); g->dt0 = now; } else if (want > 0 && net->tcp.peer_fin && net->tcp.rcount == 0) { r = web_eof(wb); if (!r) r = -1; break; } else break;
            } else {
                int n = net_tcp_read(net, b, sizeof b);
                if (n > 0) { g->dt0 = now; g->resp_len += n; r = web_feed(wb, b, n); }
                else { if (net->tcp.peer_fin && net->tcp.rcount == 0) { r = web_eof(wb); if (!r) r = -1; } break; }
            }
        }
        if (r) dir_net_result(w, r);
        break; }
    default: break;
    }
}
static void dir_stage_text(NG *g, char *o) {                                      /* what the status screen says while a page loads */
    const char *s = g->ds == DS_DNS ? TR("Procurando o site (DNS)...", "Looking up the site (DNS)...") : g->ds == DS_CONNECT ? TR("Conectando ao site...", "Connecting to the site...") : g->ds == DS_TLS ? TR("Conexao segura (TLS)...", "Secure connection (TLS)...") : g->ds == DS_SEND ? TR("Enviando o pedido...", "Sending the request...") : g->ds == DS_RECV ? TR("Baixando a pagina...", "Downloading the page...") : TR("Preparando...", "Getting ready...");
    int n = 0; while (s[n] && n < 60) { o[n] = s[n]; n++; } if (g->ds == DS_RECV && g->resp_len > 0) { o[n++] = ' '; char d[12]; int k = 0, v = g->resp_len / 1024; do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v); while (k) o[n++] = d[--k]; o[n++] = ' '; o[n++] = 'K'; o[n++] = 'B'; } o[n] = 0;
}
