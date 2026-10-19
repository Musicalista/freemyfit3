#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "webc.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = malloc(n); fread(b, 1, n, f); fclose(f);
    static Web w; strcpy(w.url, argv[2]); web_reset_response(&w); const char *hd = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"; web_feed(&w, (const uint8_t *)hd, (int)strlen(hd));
    int r = 0, pos = 0; while (pos < n && !r) { int k = n - pos > 1000 ? 1000 : (int)(n - pos); r = web_feed(&w, b + pos, k); pos += k; }
    printf("fed %d of %ld bytes, body=%d trunc=%d skip=%d name=%s hs=%d links=%d\n", pos, n, w.bl, w.trunc, w.skip_active, w.skipname, w.hs, w.nl);
    printf("%.600s\n", w.body); return 0;
}
