/* Probe 2: validate tp_sample_get. Buzzes "armed" (id 69), then for ~5 s polls the touch panel.
 * On first touch buzzes a pattern that encodes the screen quadrant: TL=1 (single), TR=4 (3x short), BL=5 (2 short+1 long), BR=7. */
typedef unsigned char u8; typedef unsigned short u16;
#define MOTOR_ONCE ((int (*)(int, int))0x2c1423f9)
#define TP_SAMPLE_GET ((int (*)(void *))0x2c0a76d9)
void probe(void) {
    u8 s[128]; for (int i = 0; i < 128; i++) s[i] = 0;
    MOTOR_ONCE(69, 0);
    for (volatile unsigned t = 0; t < 12000; t++) {
        TP_SAMPLE_GET(s);
        if (s[1] == 1 || s[1] == 2) {
            unsigned x = *(u16 *)(s + 4), y = *(u16 *)(s + 6);
            int id = (y < 201) ? (x < 128 ? 1 : 4) : (x < 128 ? 5 : 7);
            MOTOR_ONCE(id, 0);
            return;
        }
        for (volatile int k = 0; k < 30000; k++) { }
    }
}
