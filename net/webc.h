/* Tiny web client logic for the watch (original code): URL parsing/resolving, an HTTP/1.1 response parser (status, headers, chunked bodies) and a streaming
 * HTML -> text converter that produces the reader's page format ("W1 / S / U / T / body / \x01 / L<n>\t<url>"). Portable C, no libc, no globals. */
#ifndef WEBC_H
#define WEBC_H
#include <stdint.h>

#define WB_BODY 18000
#define WB_LINKS 40
#define WB_URL 150

typedef struct Web {
    char url[WB_URL + 60]; char host[96]; uint16_t port; uint8_t https; char path[WB_URL + 60];
    /* response */
    uint8_t st, trunc, chunked, enc_bad, plain, bin, latin1, hdr_done; int code; char loc[WB_URL + 60]; char ctype[40];
    char line[300]; int ll; int cst; long csz, clen, got;
    /* html converter */
    uint8_t skip_nest, main_seen, hs, in_title, a_active, pend_space, want_nl, skip_active, utf_need, quote, lastc, in_pre; uint32_t cp; char skipname[12]; char tb[420]; int tbl; char eb[12]; int ebl;
    int a_start, skip_depth; char a_href[WB_URL + 60]; int ul;                       /* ul: raw bytes seen of the current <title> */
    char title[84]; int tl; char body[WB_BODY + 80]; int bl; char links[WB_LINKS][WB_URL + 1]; int nl; char marks[WB_LINKS];
} Web;

int  web_parse_url(Web *w, const char *url);                              /* sets host/port/path/https; 0 ok, -1 not an http(s) URL */
int  web_resolve(const char *base, const char *ref, char *out, int max);  /* 0 ok, -1 if ref is not a web link (mailto:, javascript:, ...) */
int  web_request(const Web *w, char *out, int max);                       /* the GET request text; returns its length */
void web_reset_response(Web *w);                                          /* before a (new) request to w->url */
int  web_feed(Web *w, const uint8_t *d, int n);                           /* 0 more, 1 page complete, 2 redirect (w->loc), -1 unsupported (w->enc_bad / bin) */
int  web_eof(Web *w);                                                     /* the connection closed: 1 page complete, 2 redirect, -1 nothing usable */
int  web_page(const Web *w, char *out, int max, int seq);                 /* the reader's page text; returns its length */
void web_search_url(const char *query, char *out, int max);               /* DuckDuckGo HTML search URL for a free-text query */
int  web_has_scheme(const char *q);                                       /* starts with http:// or https:// */
int  web_looks_like_url(const char *q);                                   /* 1 if the text should be opened as an address instead of searched */
#endif
