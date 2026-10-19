/* midp.h: MIDP 2.0 / CLDC 1.1 runtime on top of vm.c. Same C code runs on the PC (tests) and inside the watch app. */
#ifndef MIDP_H
#define MIDP_H
#include <stdint.h>
#define MIDP_W 240
#define MIDP_H_ 320
typedef struct {
    uint8_t *(*read_file)(const char *path, uint32_t *len);          /* returns a buffer allocated with malloc (jm_alloc on the watch), or 0 */
    int (*write_file)(const char *path, const uint8_t *data, uint32_t len);
    int (*delete_file)(const char *path);
    void (*log)(const char *s);
    void (*vibrate)(int ms);
    uint32_t (*ticks)(void);                                         /* ms, never 0 */
} MidpHost;
typedef struct Midp Midp;
/* takes ownership of jar and rt (buffers allocated with malloc). id: short name used for save files. */
Midp *midp_create(const MidpHost *h, uint8_t *jar, uint32_t jarlen, uint8_t *rt, uint32_t rtlen, const char *id, uint32_t heap_bytes);
int midp_boot(Midp *m);                                              /* finds the MIDlet, starts it on a Java thread; 0 ok */
void midp_run(Midp *m, uint32_t budget_ms);                          /* give the Java threads CPU time */
void midp_event(Midp *m, int type, int arg);                         /* 1/2 key down/up (arg = key code); 3/4/5 pointer press/drag/release (arg = x | y << 16) */
const uint16_t *midp_screen(Midp *m, int *dirty);                    /* MIDP_W x MIDP_H_ RGB565; *dirty is set when it changed since the last call */
int midp_finished(Midp *m);                                          /* 1 when the MIDlet quit or crashed */
const char *midp_error(Midp *m);                                     /* last fatal error (text), or 0 */
uint32_t midp_heap_used(Midp *m);
void midp_destroy(Midp *m);
#endif
