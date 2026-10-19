#include "kbd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t now = 1000;
static uint32_t get(const char *s, int *adv) { unsigned char c = s[0]; if (c < 0x80) { *adv = 1; return c; } if ((c & 0xE0) == 0xC0) { *adv = 2; return ((c & 31) << 6) | (s[1] & 63); } *adv = 3; return ((c & 15) << 12) | ((s[1] & 63) << 6) | (s[2] & 63); }
static void act(Kbd *k, int a) { int x, y; if (!kbd_find_action(k, a, &x, &y)) exit(2); kbd_touch(k, x, y, now); now += 200; }
static void tap9(Kbd *k, int d) { int x, y; if (!kbd_find_t9(k, d, &x, &y)) exit(3); kbd_touch(k, x, y, now); now += 150; }
static const struct { int cp, base, acc; } comp[] = { {225,'a',1},{224,'a',4},{226,'a',2},{227,'a',3},{233,'e',1},{234,'e',2},{237,'i',1},{243,'o',1},{244,'o',2},{245,'o',3},{250,'u',1} };
/* type one character the way a person would: accent key first, then the multi-tap cycle; wait for the timeout afterwards */
static void type_cp(Kbd *k, int cp) {
    if (cp == ' ') { act(k, KA_SPACE); return; }
    for (unsigned i = 0; i < sizeof comp / sizeof comp[0]; i++) if (comp[i].cp == cp) { for (int a = 0; a < comp[i].acc; a++) act(k, KA_ACCENT); cp = comp[i].base; break; }
    if (cp >= 'A' && cp <= 'Z') cp += 32;
    for (int d = 1; d <= 9; d++) { int idx = kbd_t9_index(d, cp); if (idx >= 0) { for (int t = 0; t <= idx; t++) tap9(k, d); now += 1200; return; } }
    printf("no key for %d\n", cp); exit(4);
}
static void dump(const Kbd *k, const char *name) {
    static uint16_t fb[KBD_W * KBD_H]; kbd_draw(k, fb); FILE *f = fopen(name, "wb"); fprintf(f, "P6\n%d %d\n255\n", KBD_W, KBD_H);
    for (int i = 0; i < KBD_W * KBD_H; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
int main(void) {
    int fails = 0;
    Kbd k; kbd_init(&k, ""); kbd_set_msg(&k, "Maria Silva", "Oi! Voce vai chegar a que horas hoje? Preciso saber para deixar o jantar pronto e avisar o pessoal l\xc3\xa1. Responde quando puder, ok?"); dump(&k, "kbd_empty.ppm");
    const char *want = "Ol\xc3\xa1, tudo bem? Estou a caminho \xc3\xa0s 18h. Pe\xc3\xa7o desculpa pela confus\xc3\xa3o!";
    /* "à" (grave) is 4 presses of the accent key; "ç" is on key 2 (4th entry); other accents go through the accent key first */
    for (const char *p = want; *p;) {
        int adv; uint32_t c = get(p, &adv);
        if (c == 0xE0) { for (int a = 0; a < 4; a++) act(&k, KA_ACCENT); tap9(&k, 2); now += 1200; }
        else if (c == 231) { for (int t = 0; t < 4; t++) tap9(&k, 2); now += 1200; }
        else type_cp(&k, (int)c);
        p += adv;
    }
    printf("typed: %s\nlen=%d match=%d\n", k.buf, k.len, !strcmp(k.buf, want)); if (strcmp(k.buf, want)) fails++;
    dump(&k, "kbd_typed.ppm");

    Kbd m; kbd_init(&m, "");                                             /* multi-tap: quick taps cycle, a pause starts a new letter */
    tap9(&m, 2); tap9(&m, 2); now += 1200; tap9(&m, 2);
    printf("multitap 'b' then 'a' (auto caps -> \"Ba\"): %s\n", m.buf); if (strcmp(m.buf, "Ba")) fails++;
    for (int i = 0; i < 6; i++) tap9(&m, 2);                             /* wraps around the 5-entry cycle */
    printf("wrap: %s\n", m.buf);
    Kbd q; kbd_init(&q, ""); act(&q, KA_MODE); act(&q, KA_MODE); tap9(&q, 7); now += 1200; tap9(&q, 7); tap9(&q, 7);
    printf("ABC mode: %s\n", q.buf); if (strcmp(q.buf, "PQ")) fails++;
    act(&k, KA_BACK); act(&k, KA_BACK); printf("after 2x backspace: %s\n", k.buf);
    act(&k, KA_SEND); printf("state=%d (1=SEND)\n", k.state); if (k.state != KBD_SEND) fails++;
    Kbd e; kbd_init(&e, ""); act(&e, KA_SEND); printf("send on empty text: state=%d (0=still editing)\n", e.state); if (e.state != KBD_EDIT) fails++;
    act(&e, KA_CANCEL); printf("cancel state=%d\n", e.state); if (e.state != KBD_CANCEL) fails++;
    printf(fails ? "FAILED %d\n" : "ALL OK\n", fails);
    return fails;
}
