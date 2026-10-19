#include <stdio.h>
#include <string.h>
#include "webc.h"
int main(void) {
    static Web w; strcpy(w.url, "https://example.com/dir/page.html"); web_reset_response(&w);
    const char *h = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n<html><head><title>T &amp; Co</title><style>p{color:red}</style></head><body><h1>Ola</h1><p>Texto <b>bold</b> com <a href=\"/x?a=1&amp;b=2\">link um</a> e <a href='rel.html'>link dois</a>.</p><p><a href=\"https://iana.org/d\">Learn more</a></p><ul><li>um</li><li>dois &eacute; &#233; &#xE9; &euro;</li></ul><!-- coment --><script>var a = '<b>';</script><p>fim</p></body></html>";
    int r = web_feed(&w, (const uint8_t *)h, (int)strlen(h)); r = web_eof(&w); char out[4000]; int n = web_page(&w, out, sizeof out, 3);
    printf("r=%d n=%d\n", r, n); for (int i = 0; i < n; i++) { unsigned char c = (unsigned char)out[i]; if (c == '\n') puts("|"); else if (c < 32) printf("<%02x>", c); else putchar(c); } puts("");
    return 0;
}
