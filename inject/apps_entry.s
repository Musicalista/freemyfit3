@ Two hook entries in one blob (flash cave, read-only).
@ cave      : replaces `bl send_reply` @0x2c1d0802 (quick-reply click handler). kbd_hook(seq,text,len,&record) != 0 -> skip rest of handler.
@ cave_menu : replaces `bl log` @0x2c1b76a0 in app_settings_tutorials_goto_sub_page; schedules the "Apps extras" menu (r4 = page root, r6 = page).
    .syntax unified
    .thumb
    .section .text.cave, "ax"
    .global cave
    .thumb_func
cave:
    push    {r0-r4, lr}
    add     r3, sp, #216         @ &notice record = handler sp + 0xc0 (we pushed 24 bytes)
    bl      kbd_hook
    cmp     r0, #0
    pop     {r0-r4, lr}
    bne     1f
    ldr     r12, =0x2c112f59     @ original send_reply
    bx      r12
1:  ldr     r12, =0x2c1d07a3     @ handler epilogue
    bx      r12
    .ltorg

    .global cave_menu
    .thumb_func
cave_menu:
    push    {r0-r4, lr}
    mov     r0, r4
    mov     r1, r6
    bl      menu_hook
    pop     {r0-r4, lr}
    ldr     r12, =0x2c2974f9     @ original callee: log function
    bx      r12
    .ltorg
