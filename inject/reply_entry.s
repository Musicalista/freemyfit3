@ Reply-keyboard trampoline. Replaces `bl send_reply` (0x2c112f58) at 0x2c1d0802 in the quick-reply click handler.
@ kbd_hook(seq_id, text, len): 0 -> run the original send_reply; nonzero -> keyboard took over, so we leave the whole handler
@ through its normal canary-check + epilogue at 0x2c1d07a2 (skips the "sending" animation/timer that would close the page).
    .syntax unified
    .thumb
    .section .text.cave, "ax"
    .global cave
    .thumb_func
cave:
    push    {r0-r4, lr}
    add     r3, sp, #216         @ r3 = &notice record in the handler frame (handler sp + 0xc0; we pushed 24 bytes)
    bl      kbd_hook
    cmp     r0, #0
    pop     {r0-r4, lr}
    bne     1f
    ldr     r12, =0x2c112f59     @ original send_reply
    bx      r12
1:  ldr     r12, =0x2c1d07a3     @ handler epilogue (stack canary check, add sp, pop {r4-r7,pc})
    bx      r12
    .ltorg
