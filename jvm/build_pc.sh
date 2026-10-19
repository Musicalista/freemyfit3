#!/bin/sh
# builds host_pc.exe (PC test host: same runtime as the watch)
cd "$(dirname "$0")" && gcc -std=gnu99 -O1 -g -Wall -Wno-unused-function -Wno-misleading-indentation -I . -I ../kbd -o host_pc.exe host_pc.c midp.c vm.c jar.c inflate.c png.c -lm
