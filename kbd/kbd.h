/* On-screen reply keyboard for the Fit3 (256x402 RGB565): pastel T9 pad (multi-tap), portable C, no libc, UTF-8 output.
 * Usage: kbd_init(); loop { kbd_touch(x,y,now_ms) per tap; kbd_draw(fb) when dirty } until k->state != KBD_EDIT. */
#ifndef KBD_ENGINE_H
#define KBD_ENGINE_H
#include <stdint.h>

#define KBD_W 256
#define KBD_H 402
#define KBD_MAX 250            /* send_reply length is a u8: keep UTF-8 text <= 250 bytes */

enum { KBD_EDIT = 0, KBD_SEND = 1, KBD_CANCEL = 2 };

typedef struct {
    char buf[KBD_MAX + 4]; int len;      /* UTF-8, NUL terminated */
    uint8_t caps;                        /* 0 "Abc" (auto capital), 1 "abc", 2 "ABC" */
    uint8_t accent;                      /* pending accent for the NEXT letter: 0 none, 1 acute, 2 circumflex, 3 tilde, 4 grave */
    uint8_t state, has_msg;
    uint8_t en;                          /* 1: English UI text (set by the caller), 0: Portuguese */
    uint8_t last_key, last_idx, cur_upper, acc_use;   /* multi-tap state */
    uint32_t last_time;
    char title[48], body[200];           /* message being replied to (UTF-8, optional) */
} Kbd;

void kbd_init(Kbd *k, const char *initial);
void kbd_set_msg(Kbd *k, const char *title, const char *body);   /* copies (bounded) the incoming message to show above the reply */
void kbd_touch(Kbd *k, int x, int y, uint32_t now_ms);           /* one tap; now_ms drives the multi-tap timeout (900 ms) */
void kbd_draw(const Kbd *k, uint16_t *fb);                       /* full redraw into KBD_W x KBD_H buffer */
int  kbd_find_t9(const Kbd *k, int digit, int *cx, int *cy);     /* centre of T9 key 1..9 (for tests) */
int  kbd_find_action(const Kbd *k, int act, int *cx, int *cy);
int  kbd_t9_len(int digit);                                      /* number of characters on a key's cycle (for tests) */
int  kbd_t9_index(int digit, int cp);                            /* position of cp in a key's cycle, -1 if absent (for tests) */
enum { KA_T9, KA_BACK, KA_MODE, KA_ACCENT, KA_SPACE, KA_SEND, KA_CANCEL };
#endif
