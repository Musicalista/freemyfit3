ENTRY(reset_handler)
MEMORY { ROM (rx) : ORIGIN = 0x00000000, LENGTH = 4M  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 4M }
SECTIONS {
    .text : { KEEP(*(.vectors)) *(.text .text.*) *(.rodata .rodata.*) . = ALIGN(4); } > ROM
    .data : { *(.data .data.*) . = ALIGN(4); } > ROM
    .bss (NOLOAD) : { . = ALIGN(4); __bss_start = .; *(.bss .bss.*) *(COMMON) . = ALIGN(4); __bss_end = .; } > RAM
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __arena = ALIGN(__bss_end, 8);
}
