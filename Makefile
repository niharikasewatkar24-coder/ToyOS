CLANG ?= clang
LLD ?= ld.lld
NASM ?= nasm
QEMU ?= qemu-system-i386
GRUB_MKRESCUE ?= grub-mkrescue

CFLAGS := --target=i386-unknown-none-elf -march=i386 -mno-sse -mno-sse2 \
          -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector \
          -fno-builtin -Wall -Wextra

PI ?= 1
CFLAGS += -DENABLE_PRIORITY_INHERITANCE=$(PI)

LDFLAGS := -m elf_i386 -T link.ld

C_SOURCES := \
    src/c/entry.c \
    src/c/kernel/idt.c \
    src/c/kernel/gdt.c \
    src/c/kernel/exception_handler.c \
    src/c/kernel/interrupt_handler.c \
    src/c/kernel/syscall.c \
    src/c/drivers/keyboard/keyboard.c \
    src/c/drivers/timer/timer.c \
    src/c/drivers/serial_port/serial_port.c \
    src/c/drivers/ide/ide.c \
    src/c/memory/pmm.c \
    src/c/memory/paging.c \
    src/c/scheduler/scheduler.c \
    src/c/sync/mutex.c

ASM_SOURCES := \
    src/asm/boot.asm \
    src/asm/low_level_functions.asm \
    src/asm/ex_handlers.asm \
    src/asm/int_handlers.asm \
    src/asm/syscall.asm \
    src/asm/user_program.asm

C_OBJECTS := $(patsubst %.c,build/kernel/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst src/asm/%.asm,build/asm/%.o,$(ASM_SOURCES))
OBJECTS := $(ASM_OBJECTS) $(C_OBJECTS)

.PHONY: all run iso boot_iso clean

all: build/kernel.bin

build/kernel.bin: $(OBJECTS) link.ld
	$(LLD) $(LDFLAGS) $(OBJECTS) -o $@

build/kernel/%.o: %.c
	mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -c $< -o $@

build/asm/%.o: src/asm/%.asm
	mkdir -p $(@D)
	$(NASM) -f elf32 $< -o $@

run: build/kernel.bin
	$(QEMU) -kernel build/kernel.bin -serial file:performance.jsonl

iso: build/toyos.iso

build/toyos.iso: build/kernel.bin iso/boot/grub/grub.cfg
	mkdir -p build/iso/boot/grub
	cp build/kernel.bin build/iso/boot/kernel.bin
	cp iso/boot/grub/grub.cfg build/iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ build/iso

boot_iso: build/toyos.iso
	$(QEMU) -cdrom build/toyos.iso -serial file:performance.jsonl

clean:
	rm -rf build
# Track files included directly by source files
build/kernel/src/c/entry.o: src/c/functions.c src/c/commands.c
build/asm/boot.o: src/asm/checks.asm
