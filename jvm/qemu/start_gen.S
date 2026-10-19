    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb
    .section .vectors, "a"
    .word _stack_top
    .word reset_handler + 1
    .word fault_handler + 1   /* NMI */
    .word fault_handler + 1   /* HardFault */
    .word fault_handler + 1   /* MemManage */
    .word fault_handler + 1   /* BusFault */
    .word fault_handler + 1   /* UsageFault */
    .text
    .thumb_func
    .global reset_handler
reset_handler:
    ldr r0, =0xE000ED88
    ldr r1, [r0]
    orr r1, r1, #(0xF << 20)
    str r1, [r0]
    dsb
    isb
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    movs r2, #0
1:  cmp r0, r1
    bge 2f
    str r2, [r0], #4
    b 1b
2:  bl main
3:  b 3b
    .thumb_func
    .global fault_handler
fault_handler:
    movs r0, #0x18
    ldr r1, =0x20023
    bkpt 0xab
4:  b 4b
