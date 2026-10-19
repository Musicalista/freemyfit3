    .section .rodata
    .global list_data, list_end, jar_data, jar_end, rt_data, rt_end
    .balign 4
list_data: .incbin "list_gen.txt"
list_end:
    .balign 4
jar_data: .incbin "../test/hello.jar"
jar_end:
    .balign 4
rt_data: .incbin "../rt.pak"
rt_end:
