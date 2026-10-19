/* see webc.h */
#include "webc.h"

static int tw_sl(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void tw_sc(char *d, const char *s, int max) { int i = 0; while (s[i] && i < max - 1) { d[i] = s[i]; i++; } d[i] = 0; }
static char tw_lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int tw_ieq(const char *a, const char *b) { while (*a && *b) { if (tw_lc(*a) != tw_lc(*b)) return 0; a++; b++; } return !*a && !*b; }
static int tw_ipre(const char *s, const char *pre) { while (*pre) { if (tw_lc(*s) != tw_lc(*pre)) return 0; s++; pre++; } return 1; }
static int tw_hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

/* ---------------- URLs ---------------- */
int web_parse_url(Web *w, const char *url) {
    int https;
    if (tw_ipre(url, "https://")) { https = 1; url += 8; } else if (tw_ipre(url, "http://")) { https = 0; url += 7; } else return -1;
    int i = 0; while (url[i] && url[i] != '/' && url[i] != '?' && url[i] != '#' && i < 95) { w->host[i] = tw_lc(url[i]); i++; } w->host[i] = 0;
    if (!i) return -1;
    url += i; w->https = (uint8_t)https; w->port = (uint16_t)(https ? 443 : 80);
    { int c = -1; for (int k = 0; w->host[k]; k++) if (w->host[k] == ':') { c = k; break; } if (c >= 0) { int p = 0; for (int k = c + 1; w->host[k]; k++) if (w->host[k] >= '0' && w->host[k] <= '9') p = p * 10 + (w->host[k] - '0'); w->host[c] = 0; if (p > 0 && p < 65536) w->port = (uint16_t)p; } }
    if (*url == '#' || !*url) { w->path[0] = '/'; w->path[1] = 0; }
    else if (*url == '?') { w->path[0] = '/'; tw_sc(w->path + 1, url, (int)sizeof w->path - 1); }
    else tw_sc(w->path, url, (int)sizeof w->path);
    for (int k = 0; w->path[k]; k++) if (w->path[k] == '#') { w->path[k] = 0; break; }
    for (int k = 0; w->path[k]; k++) if (w->path[k] == ' ') { w->path[k] = 0; break; }
    return 0;
}
static void norm_path(char *p) {                                              /* collapse "/./" and "/../" in the path part (before '?') */
    char out[WB_URL + 60]; int o = 0, i = 0; char *q = p; while (*q && *q != '?') q++; char save[WB_URL + 60]; tw_sc(save, q, (int)sizeof save); int plen = (int)(q - p);
    while (i < plen) {
        if (p[i] == '/' ) {
            if (i + 2 < plen + 1 && p[i + 1] == '.' && (i + 2 == plen || p[i + 2] == '/')) { i += 2; if (i == plen) out[o++] = '/'; continue; }
            if (p[i + 1] == '.' && p[i + 2] == '.' && (i + 3 == plen || p[i + 3] == '/')) { while (o > 0 && out[o - 1] != '/') o--; if (o > 0) o--; i += 3; if (i == plen) out[o++] = '/'; continue; }
        }
        out[o++] = p[i++]; if (o > WB_URL + 50) break;
    }
    if (!o) out[o++] = '/';
    out[o] = 0; tw_sc(p, out, WB_URL + 60); { int n = tw_sl(p); tw_sc(p + n, save, WB_URL + 60 - n); }
}
int web_resolve(const char *base, const char *ref, char *out, int max) {
    char tmp[WB_URL + 100]; int n = 0;
    while (*ref == ' ') ref++;
    if (tw_ipre(ref, "http://") || tw_ipre(ref, "https://")) tw_sc(tmp, ref, (int)sizeof tmp);
    else if (ref[0] == '/' && ref[1] == '/') { const char *sch = tw_ipre(base, "https") ? "https:" : "http:"; tw_sc(tmp, sch, (int)sizeof tmp); n = tw_sl(tmp); tw_sc(tmp + n, ref, (int)sizeof tmp - n); }
    else {
        for (int i = 0; ref[i] && ref[i] != '/' && ref[i] != '?' && ref[i] != '#'; i++) if (ref[i] == ':') return -1;      /* mailto:, javascript:, tel: ... */
        if (!ref[0]) return -1;
        const char *o = base; int sch = tw_ipre(o, "https://") ? 8 : 7; int e = sch; while (o[e] && o[e] != '/' && o[e] != '?' && o[e] != '#') e++;
        if (ref[0] == '/') { for (int i = 0; i < e && i < (int)sizeof tmp - 1; i++) tmp[i] = o[i]; tmp[e] = 0; n = e; tw_sc(tmp + n, ref, (int)sizeof tmp - n); }
        else if (ref[0] == '?') { int q = e; while (o[q] && o[q] != '?' && o[q] != '#') q++; for (int i = 0; i < q && i < (int)sizeof tmp - 1; i++) tmp[i] = o[i]; tmp[q] = 0; n = q; tw_sc(tmp + n, ref, (int)sizeof tmp - n); }
        else { int q = e, last = e; while (o[q] && o[q] != '?' && o[q] != '#') { if (o[q] == '/') last = q; q++; } if (last == e && o[e] != '/') { tmp[0] = 0; for (int i = 0; i < e; i++) tmp[i] = o[i]; tmp[e] = '/'; last = e; } else for (int i = 0; i <= last; i++) tmp[i] = o[i];
              n = last + 1; if (last == e && o[e] != '/') n = e + 1; tw_sc(tmp + n, ref, (int)sizeof tmp - n); }
    }
    for (int i = 0; tmp[i]; i++) if (tmp[i] == '#') { tmp[i] = 0; break; }
    { int sch = tw_ipre(tmp, "https://") ? 8 : 7; int e = sch; while (tmp[e] && tmp[e] != '/' && tmp[e] != '?') e++;
      if (!tmp[e]) { if (e + 1 < (int)sizeof tmp) { tmp[e] = '/'; tmp[e + 1] = 0; } }
      else if (tmp[e] == '/') norm_path(tmp + e); }
    /* search engines wrap results: https://duckduckgo.com/l/?uddg=<percent-encoded target>&... */
    if (tw_ipre(tmp, "https://duckduckgo.com/l/?") || tw_ipre(tmp, "http://duckduckgo.com/l/?")) {
        for (int i = 0; tmp[i]; i++) if (tmp[i] == 'u' && tmp[i + 1] == 'd' && tmp[i + 2] == 'd' && tmp[i + 3] == 'g' && tmp[i + 4] == '=') {
            char dec[WB_URL + 100]; int d = 0; for (int k = i + 5; tmp[k] && tmp[k] != '&' && d < (int)sizeof dec - 1; k++) { if (tmp[k] == '%' && tw_hexv(tmp[k + 1]) >= 0 && tw_hexv(tmp[k + 2]) >= 0) { dec[d++] = (char)(tw_hexv(tmp[k + 1]) * 16 + tw_hexv(tmp[k + 2])); k += 2; } else dec[d++] = tmp[k] == '+' ? ' ' : tmp[k]; }
            dec[d] = 0; if (tw_ipre(dec, "http")) tw_sc(tmp, dec, (int)sizeof tmp); break; }
    }
    if (tw_sl(tmp) >= max) return -1;
    tw_sc(out, tmp, max); return 0;
}
int web_request(const Web *w, char *out, int max) {
    int n = 0;
#define ADD(s) do { const char *_s = (s); while (*_s && n < max - 1) out[n++] = *_s++; } while (0)
    ADD("GET "); ADD(w->path); ADD(" HTTP/1.1"); ADD("\r\nHost: "); ADD(w->host);
    if (w->port != (w->https ? 443 : 80)) { char d[8]; int k = 0, p = w->port; char t[8]; int tl = 0; do { t[tl++] = (char)('0' + p % 10); p /= 10; } while (p); while (tl) d[k++] = t[--tl]; d[k] = 0; ADD(":"); ADD(d); }
    ADD("\r\nUser-Agent: Mozilla/5.0 (compatible; Fit3Watch/1.0)\r\nAccept: text/html,application/xhtml+xml,text/plain;q=0.9,*/*;q=0.4\r\nAccept-Language: pt-BR,pt;q=0.9,en;q=0.8\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
#undef ADD
    out[n] = 0; return n;
}
int web_has_scheme(const char *q) { return tw_ipre(q, "http://") || tw_ipre(q, "https://"); }
int web_looks_like_url(const char *q) {
    if (tw_ipre(q, "http://") || tw_ipre(q, "https://")) return 1;
    int dot = 0; for (int i = 0; q[i]; i++) { if (q[i] == ' ') return 0; if (q[i] == '.' && q[i + 1] && q[i + 1] != '.') dot = 1; }
    return dot;
}
void web_search_url(const char *query, char *out, int max) {
    int n = 0; const char *pre = "https://lite.duckduckgo.com/lite/?q="; while (*pre && n < max - 1) out[n++] = *pre++;
    for (int i = 0; query[i] && n < max - 4; i++) { unsigned char c = (unsigned char)query[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.') out[n++] = (char)c;
        else if (c == ' ') out[n++] = '+';
        else { static const char hx[] = "0123456789ABCDEF"; out[n++] = '%'; out[n++] = hx[c >> 4]; out[n++] = hx[c & 15]; } }
    out[n] = 0;
}

/* ---------------- text output with whitespace collapsing ---------------- */
static void out_byte(Web *w, char c) { if (w->bl < WB_BODY) { w->body[w->bl++] = c; } else w->trunc = 1; w->lastc = (uint8_t)c; }
static void flush_ws(Web *w) {
    if (w->bl == 0) { w->want_nl = 0; w->pend_space = 0; return; }
    if (w->want_nl) { int have = 0; while (have < 2 && w->bl - 1 - have >= 0 && w->body[w->bl - 1 - have] == '\n') have++; while (have < w->want_nl) { out_byte(w, '\n'); have++; } }
    else if (w->pend_space && w->lastc != ' ' && w->lastc != '\n') out_byte(w, ' ');
    w->want_nl = 0; w->pend_space = 0;
}
static void em_nl(Web *w, int n) { if (n > w->want_nl) w->want_nl = (uint8_t)n; }
static void em_raw(Web *w, const char *s) { flush_ws(w); while (*s) out_byte(w, *s++); }
static void em_title(Web *w, char c) { if (w->tl < 80) { if (c == ' ' && (w->tl == 0 || w->title[w->tl - 1] == ' ')) return; w->title[w->tl++] = c; w->title[w->tl] = 0; } }
static void em_ascii(Web *w, char c) {
    if (w->in_title) { em_title(w, (c == '\n' || c == '\t' || c == '\r') ? ' ' : c); return; }
    if (w->skip_active) return;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v' || (c == '\n' && !w->in_pre)) { w->pend_space = 1; return; }
    if (c == '\n') { em_nl(w, 1); return; }
    flush_ws(w); out_byte(w, c);
}
static const uint16_t SUP[] = { 0xE1,0xE0,0xE2,0xE3,0xE4,0xE9,0xE8,0xEA,0xEB,0xED,0xEC,0xEE,0xEF,0xF3,0xF2,0xF4,0xF5,0xF6,0xFA,0xF9,0xFB,0xFC,0xE7,0xF1,
                                0xC1,0xC0,0xC2,0xC3,0xC4,0xC9,0xC8,0xCA,0xCD,0xCC,0xD3,0xD2,0xD4,0xD5,0xD6,0xDA,0xD9,0xDB,0xDC,0xC7,0xD1,0xBF,0xA1,0xBA,0xAA,0xB0,0xA3 };
static void em_str(Web *w, const char *s) { while (*s) em_ascii(w, *s++); }
static void em_cp(Web *w, uint32_t cp) {
    if (cp < 128) { em_ascii(w, (char)cp); return; }
    if (w->skip_active) return;
    switch (cp) {
    case 0xA0: case 0x2000: case 0x2001: case 0x2002: case 0x2003: case 0x2009: case 0x202F: case 0x3000: em_ascii(w, ' '); return;
    case 0x2018: case 0x2019: case 0x201A: case 0x2032: case 0xB4: em_ascii(w, '\''); return;
    case 0x201C: case 0x201D: case 0x201E: case 0xAB: case 0xBB: em_ascii(w, '"'); return;
    case 0x2013: case 0x2014: case 0x2212: em_ascii(w, '-'); return;
    case 0x2026: em_str(w, "..."); return;
    case 0x2022: case 0xB7: case 0x25CF: case 0x25AA: em_ascii(w, '*'); return;
    case 0xD7: em_ascii(w, 'x'); return; case 0xF7: em_ascii(w, '/'); return;
    case 0x2192: em_str(w, "->"); return; case 0x2190: em_str(w, "<-"); return;
    case 0x2122: em_str(w, "(tm)"); return; case 0xA9: em_str(w, "(c)"); return; case 0xAE: em_str(w, "(R)"); return;
    case 0x200B: case 0x200C: case 0x200D: case 0x200E: case 0x200F: case 0x2060: case 0xFEFF: case 0xAD: return;
    case 0x20AC: if (w->in_title) { em_title(w, 'E'); return; } flush_ws(w); out_byte(w, (char)0xE2); out_byte(w, (char)0x82); out_byte(w, (char)0xAC); return;
    }
    for (unsigned i = 0; i < sizeof SUP / sizeof SUP[0]; i++) if (SUP[i] == cp) {
        if (w->in_title) { em_title(w, '?'); return; }
        flush_ws(w); out_byte(w, (char)(0xC0 | (cp >> 6))); out_byte(w, (char)(0x80 | (cp & 63))); return; }
    em_ascii(w, '?');
}
static void em_byte(Web *w, uint8_t b) {                                       /* one byte of decoded text (UTF-8 or Latin-1) */
    if (w->latin1) { em_cp(w, b); return; }
    if (w->utf_need) { if ((b & 0xC0) == 0x80) { w->cp = (w->cp << 6) | (b & 63); if (--w->utf_need == 0) em_cp(w, w->cp); return; } w->utf_need = 0; }
    if (b < 0x80) em_cp(w, b);
    else if ((b & 0xE0) == 0xC0) { w->cp = b & 0x1F; w->utf_need = 1; }
    else if ((b & 0xF0) == 0xE0) { w->cp = b & 0x0F; w->utf_need = 2; }
    else if ((b & 0xF8) == 0xF0) { w->cp = b & 0x07; w->utf_need = 3; }
    else em_cp(w, '?');
}
static uint32_t ent_value(const char *e, int n) {                              /* 0 = unknown */
    if (e[0] == '#') { uint32_t v = 0; if (n > 1 && (e[1] == 'x' || e[1] == 'X')) { for (int i = 2; i < n; i++) { int h = tw_hexv(e[i]); if (h < 0) return 0; v = v * 16 + (uint32_t)h; } } else { for (int i = 1; i < n; i++) { if (e[i] < '0' || e[i] > '9') return 0; v = v * 10 + (uint32_t)(e[i] - '0'); } } return v; }
    static const struct { const char *n; uint16_t v; } T[] = { { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }, { "nbsp", 0xA0 }, { "copy", 0xA9 }, { "reg", 0xAE }, { "hellip", 0x2026 }, { "mdash", 0x2014 }, { "ndash", 0x2013 },
        { "laquo", 0xAB }, { "raquo", 0xBB }, { "lsquo", 0x2018 }, { "rsquo", 0x2019 }, { "ldquo", 0x201C }, { "rdquo", 0x201D }, { "bull", 0x2022 }, { "middot", 0xB7 }, { "times", 0xD7 }, { "euro", 0x20AC }, { "pound", 0xA3 }, { "deg", 0xB0 },
        { "aacute", 0xE1 }, { "agrave", 0xE0 }, { "acirc", 0xE2 }, { "atilde", 0xE3 }, { "eacute", 0xE9 }, { "egrave", 0xE8 }, { "ecirc", 0xEA }, { "iacute", 0xED }, { "oacute", 0xF3 }, { "ocirc", 0xF4 }, { "otilde", 0xF5 }, { "uacute", 0xFA }, { "ccedil", 0xE7 }, { "ntilde", 0xF1 },
        { "Aacute", 0xC1 }, { "Eacute", 0xC9 }, { "Iacute", 0xCD }, { "Oacute", 0xD3 }, { "Uacute", 0xDA }, { "Ccedil", 0xC7 } };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) { int k = 0; while (T[i].n[k] && k < n && T[i].n[k] == e[k]) k++; if (!T[i].n[k] && k == n) return T[i].v; }
    return 0;
}

/* ---------------- tags ---------------- */
static int add_link(Web *w, const char *raw) {                                   /* returns the link number (1..), 0 if none */
    char url[WB_URL + 100]; if (!raw[0] || raw[0] == '#' || web_resolve(w->url, raw, url, (int)sizeof url) < 0) return 0;
    if (tw_sl(url) > WB_URL) return 0;
    for (int i = 0; i < w->nl; i++) { int k = 0; while (w->links[i][k] && w->links[i][k] == url[k]) k++; if (!w->links[i][k] && !url[k]) return i + 1; }
    if (w->nl >= WB_LINKS) return 0;
    tw_sc(w->links[w->nl], url, WB_URL + 1); return ++w->nl;
}
static void attr_value(const char *tag, const char *name, char *out, int max) {  /* value of name=... inside a raw tag (entities decoded), "" if absent */
    out[0] = 0; int nl_ = tw_sl(name);
    for (int i = 0; tag[i]; i++) {
        if (i > 0 && !(tag[i - 1] == ' ' || tag[i - 1] == '\t' || tag[i - 1] == '\n' || tag[i - 1] == '"' || tag[i - 1] == '\'')) continue;
        if (!tw_ipre(tag + i, name)) continue; int k = i + nl_; while (tag[k] == ' ') k++; if (tag[k] != '=') continue; k++; while (tag[k] == ' ') k++;
        char q = (tag[k] == '"' || tag[k] == '\'') ? tag[k++] : 0; int o = 0;
        while (tag[k] && o < max - 1) { if (q ? tag[k] == q : (tag[k] == ' ' || tag[k] == '>')) break;
            if (tag[k] == '&') { int e = k + 1; while (tag[e] && tag[e] != ';' && e - k < 9) e++; if (tag[e] == ';') { uint32_t v = ent_value(tag + k + 1, e - k - 1); if (v && v < 128) { out[o++] = (char)v; k = e + 1; continue; } } }
            out[o++] = tag[k++]; }
        out[o] = 0; return;
    }
}
static const char *const SKIPS[] = { "script", "style", "noscript", "svg", "iframe", "template", "select", "button", "nav", "footer", "textarea" };
static void tag_done(Web *w) {
    char *t = w->tb; w->tb[w->tbl] = 0; int closing = 0, i = 0;
    if (t[0] == '/') { closing = 1; i = 1; }
    char name[12]; int n = 0; while (t[i] && t[i] != ' ' && t[i] != '/' && t[i] != '>' && t[i] != '\n' && t[i] != '\t' && n < 11) name[n++] = tw_lc(t[i++]); name[n] = 0;
    int selfclose = w->tbl > 0 && w->tb[w->tbl - 1] == '/';
    if (!n || name[0] == '!' || name[0] == '?') return;
    if (!closing && !selfclose && (tw_ieq(name, "div") || tw_ieq(name, "ul") || tw_ieq(name, "ol") || tw_ieq(name, "span") || tw_ieq(name, "li") || tw_ieq(name, "section") || tw_ieq(name, "table") || tw_ieq(name, "aside") || tw_ieq(name, "sup") || tw_ieq(name, "header"))) {
        static const char *const BAD[] = { "interlanguage", "vector-menu", "vector-dropdown", "vector-toc", "mw-editsection", "mw-jump", "navbox", "catlinks", "cookie", "consent", "sr-only", "visually-hidden", "display:none", "display: none", "aria-hidden=\"true\"" };
        int bad = 0; for (int q = 0; t[q] && !bad; q++) { if (tw_ipre(t + q, " hidden") && (t[q + 7] == 0 || t[q + 7] == ' ' || t[q + 7] == '=' || t[q + 7] == '/')) bad = 1; for (unsigned k = 0; k < sizeof BAD / sizeof BAD[0] && !bad; k++) if (tw_ipre(t + q, BAD[k])) bad = 1; }
        if (bad) { w->skip_active = 1; w->skip_nest = 1; w->skip_depth = 1; tw_sc(w->skipname, name, 12); return; }
    }
    if (!closing) for (unsigned k = 0; k < sizeof SKIPS / sizeof SKIPS[0]; k++) if (tw_ieq(name, SKIPS[k]) && !selfclose) { w->skip_active = 1; w->skip_nest = 0; tw_sc(w->skipname, name, 12); return; }
    if (tw_ieq(name, "title")) { w->in_title = (uint8_t)!closing; return; }
    if (tw_ieq(name, "main") && !closing && !w->main_seen) { w->main_seen = 1; w->bl = 0; w->nl = 0; w->want_nl = 0; w->pend_space = 0; w->lastc = 10; w->a_active = 0; return; }   /* the real content starts here: drop the menus before it */
    if (tw_ieq(name, "pre")) { w->in_pre = (uint8_t)!closing; em_nl(w, 1); return; }
    if (name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && !name[2]) { if (!closing) { em_nl(w, 2); em_raw(w, "# "); } else em_nl(w, 1); return; }
    if (tw_ieq(name, "li")) { if (!closing) { em_nl(w, 1); em_raw(w, "- "); } return; }
    if (tw_ieq(name, "br") || tw_ieq(name, "hr")) { em_nl(w, 1); return; }
    if (tw_ieq(name, "td") || tw_ieq(name, "th")) { if (closing) w->pend_space = 1; return; }
    if (tw_ieq(name, "a")) {
        if (!closing) { attr_value(t, "href", w->a_href, (int)sizeof w->a_href); w->a_active = 1; w->a_start = w->bl; }
        else if (w->a_active) { w->a_active = 0; if (w->bl > w->a_start && !w->in_title) { int k = add_link(w, w->a_href); if (k) { char m[8]; int l = 0; m[l++] = '['; if (k >= 10) m[l++] = (char)('0' + k / 10); m[l++] = (char)('0' + k % 10); m[l++] = ']'; m[l] = 0; flush_ws(w); for (int q = 0; q < l; q++) out_byte(w, m[q]); } } }
        return;
    }
    if (tw_ieq(name, "p") || tw_ieq(name, "div") || tw_ieq(name, "tr") || tw_ieq(name, "ul") || tw_ieq(name, "ol") || tw_ieq(name, "table") || tw_ieq(name, "section") || tw_ieq(name, "article") || tw_ieq(name, "blockquote") || tw_ieq(name, "header") || tw_ieq(name, "main") || tw_ieq(name, "aside") || tw_ieq(name, "dd") || tw_ieq(name, "dt") || tw_ieq(name, "figure") || tw_ieq(name, "details")) { em_nl(w, tw_ieq(name, "p") ? 2 : 1); return; }
}
enum { H_TEXT, H_TAG, H_COMMENT, H_ENT, H_SKIPTAG };
static void html_byte(Web *w, uint8_t c) {
    switch (w->hs) {
    case H_TEXT:
        if (c == '<') { w->hs = H_TAG; w->tbl = 0; w->quote = 0; return; }
        if (c == '&' && !w->skip_active) { w->hs = H_ENT; w->ebl = 0; return; }
        if (!w->skip_active) em_byte(w, c);
        return;
    case H_ENT:
        if (c == ';' || w->ebl >= 10 || c == ' ' || c == '<' || c == '&') {
            uint32_t v = (c == ';') ? ent_value(w->eb, w->ebl) : 0;
            if (v) em_cp(w, v); else { em_ascii(w, '&'); for (int i = 0; i < w->ebl; i++) em_ascii(w, w->eb[i]); if (c == ';') em_ascii(w, ';'); }
            w->hs = H_TEXT; if (c != ';') html_byte(w, c); return; }
        w->eb[w->ebl++] = (char)c; return;
    case H_TAG:
        if (w->skip_active && c == '<') { w->tbl = 0; return; }                    /* inside a skipped element a new '<' restarts the tag */
        if (w->tbl == 3 && w->tb[0] == '!' && w->tb[1] == '-' && w->tb[2] == '-') { w->hs = H_COMMENT; w->tbl = 0; w->quote = 0; html_byte(w, c); return; }
        if (w->quote) { if (c == (uint8_t)w->quote) w->quote = 0; }
        else if (!w->skip_active && (c == '"' || c == '\'')) { if (w->tbl > 0 && w->tb[w->tbl - 1] == '=') w->quote = (char)c; }
        else if (c == '>') {
            w->hs = H_TEXT; w->tb[w->tbl] = 0;
            if (w->skip_active) {
                int cl = w->tb[0] == '/'; const char *nm = w->tb + cl; int L = tw_sl(w->skipname);
                if (tw_ipre(nm, w->skipname) && !(tw_lc(nm[L]) >= 'a' && tw_lc(nm[L]) <= 'z')) {
                    if (cl) { if (!w->skip_nest || --w->skip_depth <= 0) w->skip_active = 0; }
                    else if (w->skip_nest && !(w->tbl > 0 && w->tb[w->tbl - 1] == '/')) w->skip_depth++;
                }
                return;
            }
            tag_done(w); return;
        }
        if (w->tbl < (int)sizeof w->tb - 1) w->tb[w->tbl++] = (char)c; return;
    default:                                                                         /* H_COMMENT: ends with --> */
        w->tb[w->tbl & 3] = (char)c; w->tbl++;
        if (c == '>' && w->tbl >= 3 && w->tb[(w->tbl - 2) & 3] == '-' && w->tb[(w->tbl - 3) & 3] == '-') { w->hs = H_TEXT; w->tbl = 0; }
        return;
    }
}
static void body_bytes(Web *w, const uint8_t *d, int n) {
    for (int i = 0; i < n && !w->trunc; i++) {
        if (w->plain) { if (d[i] == '\n') em_nl(w, 1); else em_byte(w, d[i]); } else html_byte(w, d[i]);
        if (w->bl >= WB_BODY - 4) w->trunc = 1;
    }
}

/* ---------------- HTTP response ---------------- */
void web_reset_response(Web *w) {
    w->st = 0; w->trunc = 0; w->chunked = 0; w->enc_bad = 0; w->plain = 0; w->bin = 0; w->latin1 = 0; w->hdr_done = 0; w->code = 0; w->loc[0] = 0; w->clen = -1; w->got = 0; w->ctype[0] = 0; w->ll = 0; w->cst = 0; w->csz = 0;
    w->hs = H_TEXT; w->in_title = 0; w->a_active = 0; w->pend_space = 0; w->want_nl = 0; w->skip_active = 0; w->utf_need = 0; w->quote = 0; w->lastc = '\n'; w->in_pre = 0; w->tbl = 0; w->ebl = 0; w->main_seen = 0;
    w->tl = 0; w->title[0] = 0; w->bl = 0; w->nl = 0; w->ul = 0;
}
static void header_line(Web *w, char *l) {
    if (tw_ipre(l, "content-type:")) { char *v = l + 13; while (*v == ' ') v++; tw_sc(w->ctype, v, (int)sizeof w->ctype);
        for (int i = 0; v[i]; i++) if (tw_ipre(v + i, "charset=")) { const char *c = v + i + 8; if (tw_ipre(c, "iso-8859") || tw_ipre(c, "windows-125") || tw_ipre(c, "latin")) w->latin1 = 1; } }
    else if (tw_ipre(l, "transfer-encoding:")) { for (int i = 18; l[i]; i++) if (tw_ipre(l + i, "chunked")) w->chunked = 1; }
    else if (tw_ipre(l, "content-encoding:")) { char *v = l + 17; while (*v == ' ') v++; if (!tw_ipre(v, "identity") && *v) w->enc_bad = 1; }
    else if (tw_ipre(l, "content-length:")) { const char *v = l + 15; long n = 0; while (*v == 32) v++; while (*v >= 48 && *v <= 57) n = n * 10 + (*v++ - 48); w->clen = n; }
    else if (tw_ipre(l, "location:")) { char *v = l + 9; while (*v == ' ') v++; tw_sc(w->loc, v, (int)sizeof w->loc); }
}
static int headers_end(Web *w) {                                                    /* 0 go on, 2 redirect, -1 unsupported */
    w->hdr_done = 1;
    if (w->code >= 300 && w->code < 400 && w->loc[0]) return 2;
    if (w->enc_bad) return -1;
    if (w->ctype[0]) {
        if (tw_ipre(w->ctype, "text/plain")) w->plain = 1;
        else if (!(tw_ipre(w->ctype, "text/html") || tw_ipre(w->ctype, "application/xhtml") || tw_ipre(w->ctype, "text/xml") || tw_ipre(w->ctype, "application/xml"))) { w->bin = 1; return -1; }
    }
    if (w->clen == 0 && !w->chunked) return 1;
    return 0;
}
int web_feed(Web *w, const uint8_t *d, int n) {
    int i = 0;
    while (i < n) {
        if (w->trunc) return 1;
        if (!w->hdr_done) {
            uint8_t c = d[i++]; if (c == '\r') continue;
            if (c != '\n') { if (w->ll < (int)sizeof w->line - 1) w->line[w->ll++] = (char)c; continue; }
            w->line[w->ll] = 0;
            if (w->st == 0) { int k = 0; while (w->line[k] && w->line[k] != ' ') k++; while (w->line[k] == ' ') k++; w->code = 0; while (w->line[k] >= '0' && w->line[k] <= '9') w->code = w->code * 10 + (w->line[k++] - '0'); w->st = 1; }
            else if (w->ll == 0) { int r = headers_end(w); w->ll = 0; if (r) return r; continue; }
            else header_line(w, w->line);
            w->ll = 0; continue;
        }
        if (!w->chunked) { int k = n - i; if (w->clen >= 0 && k > w->clen - w->got) k = (int)(w->clen - w->got); if (k > 0) body_bytes(w, d + i, k); w->got += k; if (w->clen >= 0 && w->got >= w->clen) return 1; return w->trunc ? 1 : 0; }
        switch (w->cst) {                                                          /* chunked transfer decoding */
        case 0: { uint8_t c = d[i++]; if (c == '\r') break; if (c == '\n') { if (w->csz == 0) { w->cst = 4; } else w->cst = 1; break; } int h = tw_hexv((char)c); if (h >= 0) w->csz = w->csz * 16 + h; else if (c == ';') w->cst = 5; break; }
        case 5: { uint8_t c = d[i++]; if (c == '\n') { w->cst = w->csz ? 1 : 4; } break; }
        case 1: { long k = w->csz; if (k > n - i) k = n - i; body_bytes(w, d + i, (int)k); i += (int)k; w->csz -= k; if (w->csz == 0) w->cst = 2; break; }
        case 2: { uint8_t c = d[i++]; if (c == '\n') { w->cst = 0; w->csz = 0; } break; }
        default: return 1;
        }
    }
    return 0;
}
int web_eof(Web *w) {
    if (!w->hdr_done) return w->st ? -1 : -1;
    if (w->code >= 300 && w->code < 400 && w->loc[0]) return 2;
    return 1;
}
int web_page(const Web *w, char *out, int max, int seq) {
    int n = 0;
#define PUT(s) do { const char *_s = (s); while (*_s && n < max - 2) out[n++] = *_s++; } while (0)
    char num[12]; int nl_ = 0; { int v = seq; char t[12]; int tl = 0; do { t[tl++] = (char)('0' + v % 10); v /= 10; } while (v); while (tl) num[nl_++] = t[--tl]; num[nl_] = 0; }
    PUT("W1\nS"); PUT(num); PUT("\nU"); { char u[121]; tw_sc(u, w->url, 121); PUT(u); } PUT("\nT"); PUT(w->title[0] ? w->title : w->host); PUT("\n");
    { int bl = w->bl; while (bl > 0 && (w->body[bl - 1] == '\n' || w->body[bl - 1] == ' ')) bl--; if (bl == 0) PUT(w->bin ? "(tipo de arquivo nao suportado)" : "(pagina sem texto)");
      for (int i = 0; i < bl && n < max - 4; i++) out[n++] = w->body[i]; }
    if (w->trunc) PUT("\n[...cortado]");
    PUT("\n\x01\n");
    for (int i = 0; i < w->nl; i++) {
        char mk[8]; int l = 0; int k = i + 1; mk[l++] = '['; if (k >= 10) mk[l++] = (char)('0' + k / 10); mk[l++] = (char)('0' + k % 10); mk[l++] = ']'; mk[l] = 0;
        int found = 0; for (int p = 0; p + l <= w->bl && !found; p++) { int m = 0; while (m < l && w->body[p + m] == mk[m]) m++; if (m == l) found = 1; }
        if (!found) continue; PUT("L"); { char t[12]; int tl = 0, v = k; do { t[tl++] = (char)('0' + v % 10); v /= 10; } while (v); while (tl) out[n++] = t[--tl]; } PUT("\t"); PUT(w->links[i]); PUT("\n");
    }
#undef PUT
    out[n] = 0; return n;
}
