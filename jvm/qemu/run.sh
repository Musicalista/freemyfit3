#!/bin/sh
# usage: run.sh game.jar   -> builds the bare-metal image with the watch's freestanding code and runs it in QEMU (Cortex-M4)
cd "$(dirname "$0")"
export PATH="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin:/c/Program Files/qemu:$PATH"
JAR=${1:-../test/hello.jar}
FL="$EXTRA_FL -DF3APP -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Os -g -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-pic -fno-exceptions -I .. -I ../../kbd"
sed "s|JARFILE|$JAR|; s|RTFILE|../rt.pak|" start.S > start_gen.S
arm-none-eabi-gcc $FL -c start_gen.S -o start.o || exit 1
for f in qemu_main.c ../midp.c ../vm.c ../jar.c ../inflate.c ../png.c ../sys.c; do arm-none-eabi-gcc $FL -c $f -o $(basename $f .c).o || exit 1; done
LIBGCC=$(arm-none-eabi-gcc -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -print-libgcc-file-name)
arm-none-eabi-ld -T link.ld -o test.elf start.o qemu_main.o midp.o vm.o jar.o inflate.o png.o sys.o "$LIBGCC" || exit 1
arm-none-eabi-size test.elf
timeout ${QEMU_TIMEOUT:-120} qemu-system-arm $QEMU_EXTRA -M mps2-an386 -cpu cortex-m4 -nographic -semihosting-config enable=on,target=native -kernel test.elf -serial null -monitor none 2>&1 | head -${QEMU_LINES:-40}
