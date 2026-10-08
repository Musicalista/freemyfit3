#include "kbd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t get(const char *s, int *adv) { unsigned char c = s[0]; if (c < 0x80) { *adv = 1; return c; } if ((c & 0xE0) == 0xC0) { *adv = 2; return ((c & 31) << 6) | (s[1] & 63); } *adv = 3; return ((c & 15) << 12) | ((s[1] & 63) << 6) | (s[2] & 63); }
static void tap(Kbd *k, int cp) { int x, y; if (!kbd_find_key(k, cp, &x, &y)) { printf("no key %d\n", cp); exit(1); } kbd_touch(k, x, y); }
static void act(Kbd *k, int a) { int x, y; if (!kbd_find_action(k, a, &x, &y)) exit(2); kbd_touch(k, x, y); }
static const struct { int cp, base, acc; } comp[] = { {225,'a',1},{224,'a',4},{226,'a',2},{227,'a',3},{233,'e',1},{234,'e',2},{237,'i',1},{243,'o',1},{244,'o',2},{245,'o',3},{250,'u',1},{231,'c',0},
    {193,'A',1},{201,'E',1},{211,'O',1},{218,'U',1},{195,'A',3},{213,'O',3},{199,'C',0} };
static void type_cp(Kbd *k, int cp) {
    int x, y;
    for (unsigned i = 0; i < sizeof comp / sizeof comp[0]; i++) if (comp[i].cp == cp && comp[i].acc) { for (int a = 0; a < comp[i].acc; a++) act(k, KA_ACCENT); cp = comp[i].base; break; }
    if (kbd_find_key(k, cp, &x, &y)) { kbd_touch(k, x, y); return; }
    if (cp == ' ') { act(k, KA_SPACE); return; }
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == 231 || cp == 199) { if (k->sym) act(k, KA_SYM); if (!kbd_find_key(k, cp, &x, &y)) act(k, KA_SHIFT); }
    else if (!k->sym) act(k, KA_SYM);
    tap(k, cp);
}
static void dump(const Kbd *k, const char *name) {
    static uint16_t fb[KBD_W * KBD_H]; kbd_draw(k, fb); FILE *f = fopen(name, "wb"); fprintf(f, "P6\n%d %d\n255\n", KBD_W, KBD_H);
    for (int i = 0; i < KBD_W * KBD_H; i++) { unsigned p = fb[i]; unsigned char c[3] = { (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31 }; fwrite(c, 1, 3, f); } fclose(f);
}
int main(void) {
    Kbd k; kbd_init(&k, ""); kbd_set_msg(&k, "Maria Silva", "Oi! Voce vai chegar a que horas hoje? Preciso saber para deixar o jantar pronto e avisar o pessoal lá. Responde quando puder, ok?"); dump(&k, "kbd_empty.ppm");
    const char *want = "Ol\xc3\xa1, tudo bem? Estou a caminho \xc3\xa0s 18h. Pe\xc3\xa7o desculpa pela confus\xc3\xa3o!";
    /* "Olá, tudo bem? Estou a caminho às 18h. Peço desculpa pela confusão!" */
    for (const char *p = want; *p;) { int adv; uint32_t c = get(p, &adv); type_cp(&k, (int)c); p += adv; }
    printf("typed: %s\nlen=%d match=%d\n", k.buf, k.len, !strcmp(k.buf, want));
    dump(&k, "kbd_typed.ppm");
    act(&k, KA_BACK); act(&k, KA_BACK); printf("after 2x backspace: %s\n", k.buf);
    act(&k, KA_SEND); printf("state=%d (1=SEND)\n", k.state);
    Kbd e; kbd_init(&e, ""); int x, y; kbd_find_action(&e, KA_CANCEL, &x, &y); kbd_touch(&e, x, y); printf("cancel state=%d\n", e.state);
    Kbd s; kbd_init(&s, ""); act(&s, KA_SYM); dump(&s, "kbd_sym.ppm");
    return !!strcmp(k.buf, want) ;
}
