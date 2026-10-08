@ Injection smoke test (user-triggered): vibrate with pattern id 4 (3x100ms) when the user opens a Settings > Vibration page.
@ Replaces `bl 0x2c2974f8` (the log call) at 0x2c1b3eb6 inside app_settings_vibrate_goto_sub_page (AZA3 main image).
@ Transparent: r0-r3, lr and the stack are restored before tail-calling the original callee (log fn takes stack args).
    .syntax unified
    .thumb
    .global cave
    .type cave, %function
cave:
    push    {r0-r4, lr}          @ 24 bytes: keeps 8-byte stack alignment
    movs    r0, #4               @ vibration pattern id (table at 0x2c2e8b74)
    movs    r1, #0
    ldr     r2, =0x2c1423f9      @ motor_once_ctrl (thumb)
    blx     r2
    pop     {r0-r4, lr}
    ldr     r12, =0x2c2974f9     @ original callee: log function (thumb)
    bx      r12
    .ltorg
