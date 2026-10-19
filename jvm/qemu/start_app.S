    .section .rodata
    .global list_data, list_end, jar_data, jar_end, rt_data, rt_end
    .balign 4
list_data: .incbin "LISTFILE"
list_end:
    .balign 4
jar_data: .incbin "JARFILE"
jar_end:
    .balign 4
rt_data: .incbin "RTFILE"
rt_end:
