/* linker script for Fit3 apps: one flat image at address 0 (text + rodata + data), bss after it; appcc.py turns the relocations into a table */
ENTRY(f3app_main)
SECTIONS
{
    . = 0;
    .text : { *(.text.f3app_main) *(.text .text.*) *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    __image_end = .;
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) }
    __bss_end = .;
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) *(.eh_frame*) }
}
