#!/bin/sh
# usage: run_app.sh game.jar -> runs the real watch app (j2me_app.c) in QEMU with a fake F3Api and scripted touches
cd "$(dirname "$0")"
export PATH="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin:/c/Program Files/qemu:$PATH"
JAR=${1:-../test/hello.jar}
FL="-DF3APP -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Os -g -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-pic -fno-exceptions -I .. -I ../../apps -I ../../kbd"
printf 'test.jar;Teste;%s\n' "$(wc -c < $JAR | tr -d ' ')" > list_gen.txt
sed "s|LISTFILE|list_gen.txt|; s|JARFILE|$JAR|; s|RTFILE|../rt.pak|" start_app.S > start_app_gen.S
arm-none-eabi-gcc $FL -c start.S -o start0.S.o 2>/dev/null
sed '/^    .section .rodata/,$d' start.S > start_gen.S
arm-none-eabi-gcc $FL -c start_gen.S -o start.o || exit 1
arm-none-eabi-gcc $FL -c start_app_gen.S -o start_app.o || exit 1
for f in qemu_app.c ../j2me_app.c ../midp.c ../vm.c ../jar.c ../inflate.c ../png.c ../sys.c; do arm-none-eabi-gcc $FL -c $f -o app_$(basename $f .c).o || exit 1; done
LIBGCC=$(arm-none-eabi-gcc -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -print-libgcc-file-name)
arm-none-eabi-ld -T link.ld -o app.elf start.o start_app.o app_*.o "$LIBGCC" 2>&1 | grep -v "RWX" 
timeout ${QEMU_TIMEOUT:-200} qemu-system-arm $QEMU_EXTRA -M mps2-an386 -cpu cortex-m4 -nographic -semihosting-config enable=on,target=native -kernel app.elf -serial null -monitor none 2>&1 | head -${QEMU_LINES:-30}
