[BITS 32]

; This is called by GRUB. GRUB has switched us to
; protected mode (32-bit) and disabled interrupts.
; If boot is successful, EAX contains 0x2BADB002.

global start

start:
    mov esp, stack_top
    push eax
    mov al, 'A'
    out 0xE9, al
    pop eax
    jmp entry_kernel

; Multiboot header must be 4-byte aligned.
ALIGN 4

multiboot:

    ; Multiboot flags
    MULTIBOOT_PAGE_ALIGN  equ 1 << 0
    MULTIBOOT_MEMORY_INFO equ 1 << 1


    MULTIBOOT_HEADER_MAGIC equ 0x1BADB002

    MULTIBOOT_HEADER_FLAGS equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO

    MULTIBOOT_CHECKSUM equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

    ; Multiboot header
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM

    ; Reserved fields
    dd 0
    dd 0
    dd 0
    dd 0
    dd 0

entry_kernel:

    ; Save GRUB Multiboot information pointer.
    mov [multiboot_info_ptr], ebx
    push eax
    mov al, 'B'
    out 0xE9, al
    pop eax

    call run_checks
    push eax
    mov al, 'C'
    out 0xE9, al
    pop eax

    extern kernel_entry
    push eax
    mov al, 'D'
    out 0xE9, al
    pop eax
    call kernel_entry

    jmp $

%include "src/asm/checks.asm"

SECTION .bss

    resb 8192

stack_top:

align 4

global multiboot_info_ptr

multiboot_info_ptr:
    resd 1
