#include "chacha.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int hex(const char *s, uint8_t *o) { if (s[0] == '-') return 0; int n = (int)strlen(s) / 2; for (int i = 0; i < n; i++) { unsigned v; sscanf(s + 2 * i, "%2x", &v); o[i] = (uint8_t)v; } return n; }
int main(void) {
    FILE *f = fopen("vectors.txt", "r"); static char line[8000]; int total = 0, bad = 0;
    while (fgets(line, sizeof line, f)) {
        char *t[5]; int k = 0; for (char *p = strtok(line, " \n"); p && k < 5; p = strtok(0, " \n")) t[k++] = p;
        static uint8_t key[32], nonce[12], aad[64], pt[2000], want[2100], got[2100], back[2100];
        hex(t[0], key); hex(t[1], nonce); int al = hex(t[2], aad), pl = hex(t[3], pt); int ol = hex(t[4], want);
        aead_seal(key, nonce, aad, al, pt, pl, got);
        int ok = ol == pl + 16 && !memcmp(got, want, (size_t)ol);
        int op = aead_open(key, nonce, aad, al, want, pl, back); ok = ok && op == 0 && !memcmp(back, pt, (size_t)pl);
        uint8_t save = want[ol - 1]; want[ol - 1] ^= 1; ok = ok && aead_open(key, nonce, aad, al, want, pl, back) != 0; want[ol - 1] = save;   /* tamper -> reject */
        if (pl) { static uint8_t cp[2100]; memcpy(cp, got, (size_t)ol); cp[0] ^= 0x80; ok = ok && aead_open(key, nonce, aad, al, cp, pl, back) != 0; }
        total++; if (!ok) { bad++; printf("FAIL pt=%d aad=%d\n", pl, al); }
    }
    /* in-place seal/open must also work */
    { uint8_t key[32] = {1}, nonce[12] = {2}, buf[100 + 16], orig[100]; for (int i = 0; i < 100; i++) buf[i] = orig[i] = (uint8_t)i;
      aead_seal(key, nonce, 0, 0, buf, 100, buf); int r = aead_open(key, nonce, 0, 0, buf, 100, buf); if (r || memcmp(buf, orig, 100)) { bad++; printf("FAIL in-place\n"); } else total++; }
    printf("%d vectors, %d failures -> %s\n", total, bad, bad ? "FAILED" : "ALL OK"); return bad;
}
