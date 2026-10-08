/* On-screen keyboard engine for the Fit3 (256x402 RGB565). Portable C, no libc, UTF-8 output.
 * Usage: kbd_init(); loop { kbd_touch(x,y) per tap; kbd_draw(fb) when dirty } until k->state != KBD_EDIT. */
#ifndef KBD_ENGINE_H
#define KBD_ENGINE_H
#include <stdint.h>

#define KBD_W 256
#define KBD_H 402
#define KBD_MAX 250            /* send_reply length is a u8: keep UTF-8 text <= 250 bytes */

enum { KBD_EDIT = 0, KBD_SEND = 1, KBD_CANCEL = 2 };

typedef struct {
    char buf[KBD_MAX + 4]; int len;      /* UTF-8, NUL terminated */
    uint8_t shift, sym, accent;          /* accent: 0 none, 1 acute, 2 circumflex, 3 tilde, 4 grave */
    uint8_t state, has_msg;
    char title[48], body[200];           /* message being replied to (UTF-8, optional) */
} Kbd;

void kbd_init(Kbd *k, const char *initial);
void kbd_set_msg(Kbd *k, const char *title, const char *body);   /* copies (bounded) the incoming message to show above the reply */
void kbd_touch(Kbd *k, int x, int y);                 /* one tap; updates text/state */
void kbd_draw(const Kbd *k, uint16_t *fb);            /* full redraw into KBD_W x KBD_H buffer */
int  kbd_find_key(const Kbd *k, int cp, int *cx, int *cy);   /* centre of key producing codepoint (for tests) */
int  kbd_find_action(const Kbd *k, int act, int *cx, int *cy);
enum { KA_CHAR, KA_SHIFT, KA_BACK, KA_SYM, KA_ACCENT, KA_SPACE, KA_SEND, KA_CANCEL };
#endif
