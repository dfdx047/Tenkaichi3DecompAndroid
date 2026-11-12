#!/bin/sh
# Builds test.elf with the decompilation's PS2 toolchain (../bt3 next to this repository).
set -e
cd "$(dirname "$0")"
T=../../../../bt3/tools
$T/ee-gcc2.96/bin/ee-gcc -O1 -G0 -fno-strict-aliasing -S test.c -o test.s
AS="$T/binutils/mips-ps2-decompals-as -EL -march=r5900 -mabi=o64 -no-pad-sections -mno-pdr -G0"
$AS ../../../../bt3/include/gcc_prelude.inc test.s -o test.o
$AS crt0.s -o crt0.o
$T/binutils/mips-ps2-decompals-ld -EL -Ttext 0x100000 -e _start -o test.elf crt0.o test.o
$T/binutils/mips-ps2-decompals-objdump -d test.elf | grep -cE "add\.s|vadd|vdiv|madd\.s|sqrt\.s"
