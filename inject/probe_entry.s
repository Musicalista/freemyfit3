@ Trampoline for C probes. Replaces `bl 0x2c2974f8` (log) at 0x2c1b3eb6 (Settings > Vibration page open).
@ Preserves r0-r3/lr/stack, runs probe(), then tail-calls the original log function.
    .syntax unified
    .thumb
    .section .text.cave, "ax"
    .global cave
    .thumb_func
cave:
    push    {r0-r4, lr}
    bl      probe
    pop     {r0-r4, lr}
    ldr     r12, =0x2c2974f9
    bx      r12
    .ltorg
