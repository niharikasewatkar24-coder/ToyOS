#!/bin/sh
# Run from the folder containing sched.c sched.h switch.S test_sched.c boot.S kernel.c
F="-m32 -ffreestanding -fno-pie -fno-stack-protector -mgeneral-regs-only -O1 -Wall"
for f in boot.S switch.S sched.c test_sched.c kernel.c; do gcc $F -c $f -o ${f%.*}.o || exit 1; done
ld -m elf_i386 -z noexecstack -Ttext 0x100000 -e _start -o kernel.elf boot.o switch.o sched.o test_sched.o kernel.o
qemu-system-i386 -kernel kernel.elf -display none -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot
